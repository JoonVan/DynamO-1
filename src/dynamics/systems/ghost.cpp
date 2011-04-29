/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
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

#include "ghost.hpp"
#include "../dynamics.hpp"
#include "../units/units.hpp"
#include "../BC/BC.hpp"
#include "../../simulation/particle.hpp"
#include "../species/species.hpp"
#include "../NparticleEventData.hpp"
#include "../ranges/include.hpp"
#include "../liouvillean/liouvillean.hpp"
#include "../../schedulers/scheduler.hpp"
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/uniform_int.hpp>
#include <magnet/xmlwriter.hpp>
#include <magnet/xmlreader.hpp>

CSysGhost::CSysGhost(const magnet::xml::Node& XML, dynamo::SimData* tmp): 
  System(tmp),
  uniformRand(Sim->ranGenerator, boost::uniform_real<>(0,1)),
  meanFreeTime(100000),
  Temp(Sim->dynamics.units().unitEnergy()),
  sqrtTemp(std::sqrt(Sim->dynamics.units().unitEnergy())),
  tune(false),
  setPoint(0.05),
  eventCount(0),
  lastlNColl(0),
  setFrequency(100),
  range(NULL)
{
  dt = HUGE_VAL;
  operator<<(XML);
  type = GAUSSIAN;
}

CSysGhost::CSysGhost(dynamo::SimData* nSim, double mft, double t, 
		     std::string nName):
  System(nSim),
  uniformRand(Sim->ranGenerator,boost::uniform_real<>(0,1)),
  meanFreeTime(mft),
  Temp(t),
  tune(true),
  setPoint(0.05),
  eventCount(0),
  lastlNColl(0),
  setFrequency(100),
  range(new CRAll(Sim))
{
  sysName = nName;
  type = GAUSSIAN;
}

void 
CSysGhost::runEvent() const
{
  ++Sim->eventCount;
  ++eventCount;

  if (tune && (eventCount > setFrequency))
    {
      meanFreeTime *= static_cast<double>(eventCount)
	/ ((Sim->eventCount - lastlNColl) * setPoint);

      lastlNColl = Sim->eventCount;
      eventCount = 0;
    }

  double locdt = dt;
  
#ifdef dynamo_DEBUG 
  if (boost::math::isnan(locdt))
    M_throw() << "A NAN system event time has been found";
#endif
    
  Sim->dSysTime += locdt;
    
  Sim->ptrScheduler->stream(locdt);
  
  Sim->dynamics.stream(locdt);

  locdt +=  Sim->freestreamAcc;
  Sim->freestreamAcc = 0;

  dt = getGhostt();

  unsigned int step = boost::variate_generator
    <dynamo::baseRNG&, boost::uniform_int<unsigned int> >
    (Sim->ranGenerator, 
     boost::uniform_int<unsigned int>(0, range->size() - 1))();

  const Particle& part(Sim->particleList[*(range->begin()+step)]);

  //Run the collision and catch the data
  NEventData SDat(Sim->dynamics.getLiouvillean().randomGaussianEvent
		      (part, sqrtTemp));
  
  Sim->signalParticleUpdate(SDat);

  Sim->ptrScheduler->fullUpdate(part);
  
  BOOST_FOREACH(magnet::ClonePtr<OutputPlugin>& Ptr, Sim->outputPlugins)
    Ptr->eventUpdate(*this, SDat, locdt);

}

void 
CSysGhost::initialise(size_t nID)
{
  ID = nID;
  meanFreeTime /= Sim->N;
  dt = getGhostt();
  sqrtTemp = sqrt(Temp);
}

void 
CSysGhost::operator<<(const magnet::xml::Node& XML)
{
  if (strcmp(XML.getAttribute("Type"),"Andersen"))
    M_throw() << "Attempting to load Andersen from non Andersen entry"; 
  
  try {
    meanFreeTime = XML.getAttribute("MFT").as<double>() * Sim->dynamics.units().unitTime();
    Temp = XML.getAttribute("Temperature").as<double>() * Sim->dynamics.units().unitEnergy();
    sysName = XML.getAttribute("Name");

    if (XML.getAttribute("SetFrequency").valid() && XML.getAttribute("SetPoint").valid())
      {
	tune = true;
	setFrequency = XML.getAttribute("SetFrequency").as<unsigned long long>();
	setPoint = boost::lexical_cast<double>(XML.getAttribute("SetPoint"));
      }

    range.set_ptr(CRange::getClass(XML,Sim));
  }
  catch (boost::bad_lexical_cast &)
    {
      M_throw() << "Failed a lexical cast in CGGlobal";
    }
}

void 
CSysGhost::outputXML(xml::XmlStream& XML) const
{
  XML << xml::tag("System")
      << xml::attr("Type") << "Andersen"
      << xml::attr("Name") << sysName
      << xml::attr("MFT") << meanFreeTime
    * Sim->N
    / Sim->dynamics.units().unitTime()
      << xml::attr("Temperature") << Temp 
    / Sim->dynamics.units().unitEnergy();
  
  if (tune)
    XML << xml::attr("SetPoint") << setPoint
	<< xml::attr("SetFrequency") << setFrequency;
  
  XML << range
      << xml::endtag("System");
}

double 
CSysGhost::getGhostt() const
{ 
  return  - meanFreeTime * log(1.0-uniformRand());
}

double 
CSysGhost::getReducedTemperature() const
{
  return Temp / Sim->dynamics.units().unitEnergy();
}
