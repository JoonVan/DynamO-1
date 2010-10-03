/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2010  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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


#include "rdotv.hpp"
#include "../../dynamics/include.hpp"
#include <boost/foreach.hpp>
#include "../../base/is_simdata.hpp"
#include "../0partproperty/collMatrix.hpp"
#include "../../extcode/xmlwriter.hpp"

OPRdotV::OPRdotV(const DYNAMO::SimData* tmp, const XMLNode&):
  OutputPlugin(tmp, "RdotV")
{}

void 
OPRdotV::initialise()
{}

void 
OPRdotV::eventUpdate(const IntEvent& iEvent, const PairEventData& pDat)
{
  size_t speciesIDlow = pDat.particle1_.getSpecies().getID(), 
    speciesIDhigh =pDat.particle2_.getSpecies().getID();
  
  if (speciesIDlow > speciesIDhigh) 
    std::swap(speciesIDhigh,speciesIDlow);

  mapdata& ref = rvdotacc[mapKey(iEvent.getType(), getClassKey(iEvent),
				 speciesIDlow, speciesIDhigh)];

  ref.addVal(pDat.rij | pDat.particle1_.getDeltaP());
  ref.costheta.addVal(pDat.rij | pDat.vijold 
		      / (pDat.rij.nrm() * pDat.vijold.nrm()));
}

void 
OPRdotV::eventUpdate(const GlobalEvent& globEvent, const NEventData& SDat)
{
  BOOST_FOREACH(const PairEventData& pDat, SDat.L2partChanges)
    {
      size_t speciesIDlow = pDat.particle1_.getSpecies().getID(), 
	speciesIDhigh =pDat.particle2_.getSpecies().getID();
      
      if (speciesIDlow > speciesIDhigh) 
	std::swap(speciesIDhigh,speciesIDlow);

      mapdata& ref = rvdotacc[mapKey(globEvent.getType(), 
				     getClassKey(globEvent),
				     speciesIDlow, speciesIDhigh)];
      
      ref.addVal(pDat.rij | pDat.particle1_.getDeltaP());

      ref.costheta.addVal(pDat.rij | pDat.vijold / (pDat.rij.nrm() * pDat.vijold.nrm()));
    }
}

void 
OPRdotV::eventUpdate(const LocalEvent& localEvent, const NEventData& SDat)
{
  BOOST_FOREACH(const PairEventData& pDat, SDat.L2partChanges)
    {
      size_t speciesIDlow = pDat.particle1_.getSpecies().getID(), 
	speciesIDhigh =pDat.particle2_.getSpecies().getID();
      
      if (speciesIDlow > speciesIDhigh) 
	std::swap(speciesIDhigh,speciesIDlow);
      
      mapdata& ref = rvdotacc[mapKey(localEvent.getType(), 
				     getClassKey(localEvent),
				     speciesIDlow, speciesIDhigh)];
      
      ref.addVal(pDat.rij | pDat.particle1_.getDeltaP());

      ref.costheta.addVal(pDat.rij | pDat.vijold / (pDat.rij.nrm() * pDat.vijold.nrm()));
    }
}

void
OPRdotV::eventUpdate(const System& sysEvent, const NEventData& SDat, const Iflt&)
{
  BOOST_FOREACH(const PairEventData& pDat, SDat.L2partChanges)
    {
      size_t speciesIDlow = pDat.particle1_.getSpecies().getID(), 
	speciesIDhigh =pDat.particle2_.getSpecies().getID();
      
      if (speciesIDlow > speciesIDhigh) 
	std::swap(speciesIDhigh,speciesIDlow);
      
      mapdata& ref = rvdotacc[mapKey(sysEvent.getType(), 
				     getClassKey(sysEvent),
				     speciesIDlow, speciesIDhigh)];
      
      ref.addVal(pDat.rij | pDat.particle1_.getDeltaP());

      ref.costheta.addVal(pDat.rij | pDat.vijold / (pDat.rij.nrm() * pDat.vijold.nrm()));
    } 
}

void
OPRdotV::output(xml::XmlStream &XML)
{
  XML << xml::tag("RdotV");
  
  typedef std::pair<const mapKey, mapdata> mappair;

  BOOST_FOREACH(const mappair& pair1, rvdotacc)
    {
      XML << xml::tag("Element")
	  << xml::attr("Type") 
	  << pair1.first.get<0>()
	  << xml::attr("EventName") 
	  << getName(pair1.first.get<1>(), Sim)
	  << xml::attr("Species1")
	  << Sim->dynamics.getSpecies()[pair1.first.get<2>()]->getName()
	  << xml::attr("Species2")
	  << Sim->dynamics.getSpecies()[pair1.first.get<3>()]->getName()
	  << xml::attr("RijdotDeltaMomentum") << pair1.second.getAvg()
	/ (Sim->dynamics.units().unitVelocity() 
	   * Sim->dynamics.units().unitLength()
	   * Sim->dynamics.units().unitMass());
      
      pair1.second.costheta.outputHistogram(XML, 1.0);
      
      XML << xml::endtag("Element");
    }

    XML << xml::endtag("RdotV");
}
