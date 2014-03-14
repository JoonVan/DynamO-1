/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.dynamomd.org
    Copyright (C) 2011  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <dynamo/interactions/dumbbells.hpp>
#include <dynamo/interactions/intEvent.hpp>
#include <dynamo/dynamics/dynamics.hpp>
#include <dynamo/dynamics/compression.hpp>
#include <dynamo/units/units.hpp>
#include <dynamo/simulation.hpp>
#include <dynamo/2particleEventData.hpp>
#include <dynamo/BC/BC.hpp>
#include <dynamo/ranges/IDRange.hpp>
#include <dynamo/schedulers/scheduler.hpp>
#include <dynamo/NparticleEventData.hpp>
#include <dynamo/outputplugins/outputplugin.hpp>
#include <dynamo/species/sphericalTop.hpp>
#include <magnet/xmlwriter.hpp>
#include <magnet/xmlreader.hpp>
#include <magnet/intersection/offcentre_spheres.hpp>

namespace dynamo {
  IDumbbells::IDumbbells(const magnet::xml::Node& XML, dynamo::Simulation* tmp):
    ICapture(tmp, NULL)
  {
    operator<<(XML);
  }

  void 
  IDumbbells::initialise(size_t nID)
  {
    Interaction::initialise(nID);
    ICapture::initCaptureMap();
  }

  std::array<double, 4> IDumbbells::getGlyphSize(size_t ID) const
  { 
    return {{_diamA->getProperty(ID), _diamB->getProperty(ID), _LA->getProperty(ID), _LB->getProperty(ID)}};
  }

  double IDumbbells::getExcludedVolume(size_t ID) const 
  {
    double diamA = _diamA->getProperty(ID);
    double diamB = _diamB->getProperty(ID);
    double LA = _LA->getProperty(ID);
    double LB = _LB->getProperty(ID);
    double Volume = (diamA * diamA * diamA + diamB * diamB * diamB) * M_PI / 6.0;

    //If the spheres are overlapping, subtract the volume of the lense they form from the volume of the spheres
    double d = LA + LB;
    double r = diamA;
    double R = diamB;
    if (d < (diamA + diamB) / 2)
      Volume -= M_PI * (R + r - d) * (R + r - d) * (d * d + 2 * d * r -  3 * r * r + 2 * d * R + 6 * r * R - 3 * R * R) / (12 * d);
    return Volume;
  }

  void 
  IDumbbells::operator<<(const magnet::xml::Node& XML)
  { 
    Interaction::operator<<(XML);
    _diamA = Sim->_properties.getProperty(XML.getAttribute("DiameterA"), Property::Units::Length());
    _diamB = Sim->_properties.getProperty(XML.getAttribute("DiameterB"), Property::Units::Length());
    _LA = Sim->_properties.getProperty(XML.getAttribute("LA"), Property::Units::Length());
    _LB = Sim->_properties.getProperty(XML.getAttribute("LB"), Property::Units::Length());
    _e = Sim->_properties.getProperty(XML.getAttribute("Elasticity"), Property::Units::Dimensionless());
    intName = XML.getAttribute("Name");
    if (XML.hasAttribute("UnusedDimension"))
      _unusedDimension = XML.getAttribute("UnusedDimension").as<size_t>();
    ICapture::loadCaptureMap(XML);   
  }

  double 
  IDumbbells::maxIntDist() const 
  { 
    double l = std::max(2 * _LA->getMaxValue() + _diamA->getMaxValue(), 
			2 * _LB->getMaxValue() + _diamB->getMaxValue());
    return l;
  }

  IntEvent 
  IDumbbells::getEvent(const Particle &p1, const Particle &p2) const
  {
#ifdef DYNAMO_DEBUG
    if (!Sim->dynamics->isUpToDate(p1))
      M_throw() << "Particle 1 is not up to date";
  
    if (!Sim->dynamics->isUpToDate(p2))
      M_throw() << "Particle 2 is not up to date";

    if (p1 == p2)
      M_throw() << "You shouldn't pass p1==p2 events to the interactions!";
#endif
  
    const double lA1 = _LA->getProperty(p1),
      lB1 = _LB->getProperty(p1),
      diamA1 = _diamA->getProperty(p1),
      diamB1 = _diamB->getProperty(p1);

    const double lA2 = _LA->getProperty(p2), lB2 = _LB->getProperty(p2),
      diamA2 = _diamA->getProperty(p2), diamB2 = _diamB->getProperty(p2);

    const double l1 = std::max(lA1 + 0.5 * diamA1, lB1 + 0.5 * diamB1);
    const double l2 = std::max(lA2 + 0.5 * diamA2, lB2 + 0.5 * diamB2);
    const double max_dist = l1 + l2;

    if (isCaptured(p1, p2))
      {
	//Run this to determine when the spheres no longer intersect
	const double upper_limit = Sim->dynamics->SphereSphereOutRoot(p1, p2, max_dist);
	//Test all pairings, selecting 
	std::pair<bool, double> current = Sim->dynamics->getOffcentreSpheresCollision(lA1, diamA1, lA2, diamA2, p1, p2, upper_limit, max_dist);

	std::pair<bool, double> AB = Sim->dynamics->getOffcentreSpheresCollision(lA1, diamA1, -lB2, diamB2, p1, p2, std::min(upper_limit, current.second), max_dist);
	if (AB.second < current.second) current = AB;

	std::pair<bool, double> BA = Sim->dynamics->getOffcentreSpheresCollision(-lB1, diamB1, lA2, diamA2, p1, p2, std::min(upper_limit, current.second), max_dist);
	if (BA.second < current.second) current = BA;

	std::pair<bool, double> BB = Sim->dynamics->getOffcentreSpheresCollision(-lB1, diamB1, -lB2, diamB2, p1, p2, std::min(upper_limit, current.second), max_dist);
	if (BB.second < current.second) current = BB;

	//Check if they miss each other
	if (current.second == HUGE_VAL) return IntEvent(p1, p2, upper_limit, NBHOOD_OUT, *this);
	
	//Something happens in the time interval
	if (current.first) //Its a collision!
	  return IntEvent(p1, p2, current.second, CORE, *this);
	else //Its a virtual event, we need to recalculate in a bit
	  return IntEvent(p1, p2, current.second, VIRTUAL, *this);
      }
    else 
      {
	double dt = Sim->dynamics->SphereSphereInRoot(p1, p2, max_dist);

	if (dt != HUGE_VAL)
	  return IntEvent(p1, p2, dt, NBHOOD_IN, *this);
      }

    return IntEvent(p1, p2, HUGE_VAL, NONE, *this);
  }

  PairEventData
  IDumbbells::runEvent(Particle& p1, Particle& p2, const IntEvent& iEvent)
  {
    PairEventData retval;

    switch (iEvent.getType())
      {
      case CORE:
	{
	  ++Sim->eventCount;
	  Sim->dynamics->updateParticlePair(p1, p2);
	  shared_ptr<SpSphericalTop> sp1 = std::dynamic_pointer_cast<SpSphericalTop>(Sim->species(p1));
	  shared_ptr<SpSphericalTop> sp2 = std::dynamic_pointer_cast<SpSphericalTop>(Sim->species(p2));

	  if (!sp1 || !sp2)
	    M_throw() << "Could not find the intertia of one of the particles undergoing an interaction";
	  
	  const double lA1 = _LA->getProperty(p1),
	    lB1 = _LB->getProperty(p1),
	    diamA1 = _diamA->getProperty(p1),
	    diamB1 = _diamB->getProperty(p1);
	  
	  const double lA2 = _LA->getProperty(p2),
	    lB2 = _LB->getProperty(p2),
	    diamA2 = _diamA->getProperty(p2),
	    diamB2 = _diamB->getProperty(p2);

	  const Vector director1 = Sim->dynamics->getRotData(p1).orientation * Quaternion::initialDirector();
	  const Vector director2 = Sim->dynamics->getRotData(p2).orientation * Quaternion::initialDirector();
	  const Vector angvel1 = Sim->dynamics->getRotData(p1).angularVelocity;
	  const Vector angvel2 = Sim->dynamics->getRotData(p2).angularVelocity;

	  const double m1 = sp1->getMass(p1.getID());
	  const double m2 = sp2->getMass(p2.getID());

	  const double I1 = sp1->getScalarMomentOfInertia(p1.getID());
	  const double I2 = sp2->getScalarMomentOfInertia(p2.getID());
	  
	  retval = PairEventData(p1, p2, *sp1, *sp2, CORE);
	  Sim->BCs->applyBC(retval.rij, retval.vijold);

	  double growthrate = 0;
	  if (std::dynamic_pointer_cast<DynCompression>(Sim->dynamics))
	    growthrate = std::static_pointer_cast<DynCompression>(Sim->dynamics)->getGrowthRate();
	  
	  const double growthfactor = 1 + growthrate * Sim->systemTime;

	  //Determine the colliding pair, they should almost be in contact
	  double AA_dist = std::abs(0.5 * (diamA1 + diamA2) * growthfactor - (retval.rij + growthfactor * (director1 * lA1 - director2 * lA2)).nrm());
	  double AB_dist = std::abs(0.5 * (diamA1 + diamB2) * growthfactor - (retval.rij + growthfactor * (director1 * lA1 + director2 * lB2)).nrm());
	  double BA_dist = std::abs(0.5 * (diamB1 + diamA2) * growthfactor - (retval.rij + growthfactor * (-director1 * lB1 - director2 * lA2)).nrm());
	  double BB_dist = std::abs(0.5 * (diamB1 + diamB2) * growthfactor - (retval.rij + growthfactor * (-director1 * lB1 + director2 * lB2)).nrm());

	  double l1 = lA1, l2 = lA2, d1 = diamA1, d2 = diamA2, min_dist = AA_dist;
	  if (AB_dist < min_dist)
	    {
	      l1 = +lA1;
	      l2 = -lB2;
	      d1 = diamA1;
	      d2 = diamB2;
 	      min_dist = AB_dist;
	    }

	  if (BA_dist < min_dist)
	    {
	      l1 = -lB1;
	      l2 = +lA2;
	      d1 = diamB1;
	      d2 = diamA2;
 	      min_dist = BA_dist;
	    }

	  if (BB_dist < min_dist)
	    {
	      l1 = -lB1;
	      l2 = -lB2;
	      d1 = diamB1;
	      d2 = diamB2;
 	      min_dist = BB_dist;
	    }

	  const Vector u1 = director1 * l1 * growthfactor;
	  const Vector u2 = director2 * l2 * growthfactor;
	  Vector nhat = retval.rij + u1 - u2;
	  nhat /= nhat.nrm();
	  const Vector r1 = u1 - nhat * 0.5 * d1 * growthfactor;
	  const Vector r2 = u2 + nhat * 0.5 * d2 * growthfactor;
	  
	  Vector vc12 = retval.vijold + (angvel1 ^ r1) - (angvel2 ^ r2) + growthrate * (director1 * l1 - director2 * l2 - nhat * (d1 + d2) * 0.5);
	  
	  const double e = _e->getProperty(p1, p2);
	  const double J = (1 + e) * (nhat | vc12) / ((1 / m1) + (1 / m2)+ (nhat | ((1 / I1) * ((u1 ^ nhat) ^ u1) + (1 / I2) * ((u2 ^ nhat) ^ u2))));

	  retval.rvdot = (retval.rij | retval.vijold);
	  retval.impulse = J * nhat;

	  p1.getVelocity() -= retval.impulse / m1;
	  p2.getVelocity() += retval.impulse / m2;
	  Sim->dynamics->getRotData(p1).angularVelocity -= (r1 ^ retval.impulse) / I1;
	  Sim->dynamics->getRotData(p2).angularVelocity += (r2 ^ retval.impulse) / I2;

	  
	  if (_unusedDimension != std::numeric_limits<size_t>::max())
	    {
	      p1.getVelocity()[_unusedDimension] = 0;
	      p2.getVelocity()[_unusedDimension] = 0;
	      Sim->dynamics->getRotData(p1).angularVelocity[(_unusedDimension + 1) % 3] = 0;
	      Sim->dynamics->getRotData(p1).angularVelocity[(_unusedDimension + 2) % 3] = 0;
	      Sim->dynamics->getRotData(p2).angularVelocity[(_unusedDimension + 1) % 3] = 0;
	      Sim->dynamics->getRotData(p2).angularVelocity[(_unusedDimension + 2) % 3] = 0;
	    }
	  break;
	}
      case NBHOOD_IN:
	{
	  ICapture::add(p1, p2);
	  retval = PairEventData(p1, p2, *Sim->species(p1), *Sim->species(p2), VIRTUAL);
	  iEvent.setType(VIRTUAL);
	  break;
	}
      case NBHOOD_OUT:
	{
	  ICapture::remove(p1, p2);
	  retval = PairEventData(p1, p2, *Sim->species(p1), *Sim->species(p2), VIRTUAL);
	  iEvent.setType(VIRTUAL);
	  break;
	}
      case VIRTUAL:
	{
	  retval = PairEventData(p1, p2, *Sim->species(p1), *Sim->species(p2), VIRTUAL);
	  iEvent.setType(VIRTUAL);
	  break;
	}
      default:
	M_throw() << "Unknown collision type";
      }

    return retval;
  }
   
  void 
  IDumbbells::outputXML(magnet::xml::XmlStream& XML) const
  {
    XML << magnet::xml::attr("Type") << "Dumbbells"
	<< magnet::xml::attr("Elasticity") << _e->getName()
	<< magnet::xml::attr("DiameterA") << _diamA->getName()
	<< magnet::xml::attr("DiameterB") << _diamB->getName()
	<< magnet::xml::attr("LA") << _LA->getName()
	<< magnet::xml::attr("LB") << _LB->getName()
	<< magnet::xml::attr("Name") << intName;
    
    if (_unusedDimension != std::numeric_limits<size_t>::max())
      XML << magnet::xml::attr("UnusedDimension") << _unusedDimension;
      XML << range;
    ICapture::outputCaptureMap(XML);
  }

  size_t
  IDumbbells::captureTest(const Particle& p1, const Particle& p2) const
  {
    if (&(*(Sim->getInteraction(p1, p2))) != this) return false;

    const double lA1 = _LA->getProperty(p1),
      lB1 = _LB->getProperty(p1),
      diamA1 = _diamA->getProperty(p1),
      diamB1 = _diamB->getProperty(p1);

    const double lA2 = _LA->getProperty(p2),
      lB2 = _LB->getProperty(p2),
      diamA2 = _diamA->getProperty(p2),
      diamB2 = _diamB->getProperty(p2);

    const double l1 = std::max(lA1 + 0.5 * diamA1, lB1 + 0.5 * diamB1);
    const double l2 = std::max(lA2 + 0.5 * diamA2, lB2 + 0.5 * diamB2);
    const double max_dist = l1 + l2;
    
    return Sim->dynamics->sphereOverlap(p1, p2, max_dist) > 0;
  }

  namespace {
    inline double overlap(const Vector dist, double diam)
    {
      return std::sqrt(std::max(diam * diam - (dist | dist), 0.0));
    }
  }

  bool
  IDumbbells::validateState(const Particle& p1, const Particle& p2, bool textoutput) const
  {
    double growthfactor = 1;
    if (std::dynamic_pointer_cast<DynCompression>(Sim->dynamics))
      growthfactor = (1 + std::static_pointer_cast<DynCompression>(Sim->dynamics)->getGrowthRate() * Sim->systemTime);

    const double lA1 = growthfactor * _LA->getProperty(p1),
      lB1 = growthfactor * _LB->getProperty(p1),
      diamA1 = growthfactor * _diamA->getProperty(p1),
      diamB1 = growthfactor * _diamB->getProperty(p1);
    const Vector director1 = Sim->dynamics->getRotData(p1).orientation * Quaternion::initialDirector();

    const double lA2 = growthfactor * _LA->getProperty(p2),
      lB2 = growthfactor * _LB->getProperty(p2),
      diamA2 = growthfactor * _diamA->getProperty(p2),
      diamB2 = growthfactor * _diamB->getProperty(p2);
    const Vector director2 = Sim->dynamics->getRotData(p2).orientation * Quaternion::initialDirector();

    const double l1 = std::max(lA1 + 0.5 * diamA1, lB1 + 0.5 * diamB1);
    const double l2 = std::max(lA2 + 0.5 * diamA2, lB2 + 0.5 * diamB2);
    const double max_dist = l1 + l2;

    Vector r12 = p1.getPosition() - p2.getPosition();
    Sim->BCs->applyBC(r12);
    
    bool has_error = false;
    double error;
    double distance = Sim->BCs->getDistance(p1, p2);

    if (isCaptured(p1, p2))
      {
	//Check the capture map is valid
	if (distance > max_dist)
	  {	    
	    if (textoutput)
	      derr << "Particle " << p1.getID() << " and Particle " << p2.getID() 
		   << " are registered as being closer than " << max_dist / Sim->units.unitLength()
		   << " but they're outside of this by " 
		   << (distance - max_dist) / Sim->units.unitLength()
		   << std::endl;
	    has_error = true;
	  }

	//Check if any of the spheres are overlapping
	if ((error = overlap(r12 + director1 * lA1 - director2 * lA2, (diamA1 + diamA2) / 2)))
	  {
	    if (textoutput)
	      derr << "Particle " << p1.getID() << " sphere A and Particle " << p2.getID() 
		   << " sphere A are overlapping by " << error/Sim->units.unitLength()
		   << std::endl;
	    has_error = true;
	  }
	if ((error = overlap(r12 + director1 * lA1 + director2 * lB2, (diamA1 + diamB2) / 2)))
	  {
	    if (textoutput)
	      derr << "Particle " << p1.getID() << " sphere A and Particle " << p2.getID() 
		   << " sphere B are overlapping by " << error/Sim->units.unitLength()
		   << std::endl;
	    has_error = true;
	  }
	if ((error = overlap(r12 - director1 * lB1 - director2 * lA2, (diamB1 + diamA2) / 2)))
	  {
	    if (textoutput)
	      derr << "Particle " << p1.getID() << " sphere B and Particle " << p2.getID() 
		   << " sphere A are overlapping by " << error/Sim->units.unitLength()
		   << std::endl;
	    has_error = true;
	  }
	if ((error = overlap(r12 - director1 * lB1 + director2 * lB2, (diamB1 + diamB2) / 2)))
	  {
	    if (textoutput)
	      derr << "Particle " << p1.getID() << " sphere B and Particle " << p2.getID() 
		   << " sphere B are overlapping by " << error/Sim->units.unitLength()
		   << std::endl;
	    has_error = true;
	  }
      }
    else if (distance < max_dist)
      {
	if (textoutput)
	  derr << "Particle " << p1.getID() << " and Particle " << p2.getID() 
	       << " are closer than " << max_dist / Sim->units.unitLength()
	       << " but they've not been registered as captured, despite being at a distance of " 
	       << (distance - max_dist) / Sim->units.unitLength()
	       << std::endl;
	has_error = true;
      }

    return has_error;
  }
}
