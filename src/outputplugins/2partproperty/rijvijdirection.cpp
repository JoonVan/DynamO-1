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


#include "rijvijdirection.hpp"
#include "../../dynamics/include.hpp"
#include <boost/foreach.hpp>
#include "../../base/is_simdata.hpp"
#include "../0partproperty/collMatrix.hpp"

OPRijVij::OPRijVij(const DYNAMO::SimData* tmp, const XMLNode&):
  OutputPlugin(tmp, "RdotV")
{}

void 
OPRijVij::initialise()
{}

void 
OPRijVij::process2PED(mapdata& ref, const C2ParticleData& PDat)
{
  Vector  rijnorm(PDat.rij / PDat.rij.nrm());
  Vector  vijnorm(PDat.vijold / PDat.vijold.nrm());

  Iflt rvdot(rijnorm | vijnorm);

  for (size_t iDim(0); iDim < NDIM; ++iDim)
    {
      ref.rij[iDim].addVal(rijnorm[iDim]);
      ref.vij[iDim].addVal(vijnorm[iDim]);

      size_t id1(static_cast<size_t>((rijnorm[iDim] + 1.0) * 1000));
      size_t id2(static_cast<size_t>(-rvdot * 1000.0));

      ++ref.rijcostheta[iDim].at(id1).first;
      ref.rijcostheta[iDim].at(id1).second += rvdot;

      ++ref.costhetarij[iDim].at(id2).first;
      ref.costhetarij[iDim].at(id2).second += std::fabs(rijnorm[iDim]);


      id1 = static_cast<size_t>((rijnorm[iDim] + 1.0) * 100);
      id2 = static_cast<size_t>(-rvdot * 100.0);

      ++ref.anglemapcount;
      ++ref.anglemap[iDim][id1][id2];
    }
}

void 
OPRijVij::eventUpdate(const CIntEvent& iEvent, const C2ParticleData& pDat)
{
  
  process2PED(rvdotacc[mapKey(iEvent.getType(), 
			      getClassKey(iEvent))], pDat);
}

void 
OPRijVij::eventUpdate(const CGlobEvent& globEvent, const CNParticleData& SDat)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    process2PED(rvdotacc[mapKey(globEvent.getType(), getClassKey(globEvent))],
		pDat);
}

void 
OPRijVij::eventUpdate(const CLocalEvent& localEvent, const CNParticleData& SDat)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    process2PED(rvdotacc[mapKey(localEvent.getType(), getClassKey(localEvent))],
		pDat);
}

void
OPRijVij::eventUpdate(const CSystem& sysEvent, const CNParticleData& SDat, const Iflt&)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    process2PED(rvdotacc[mapKey(sysEvent.getType(), getClassKey(sysEvent))],
		pDat);
}

void
OPRijVij::output(xmlw::XmlStream &XML)
{
  XML << xmlw::tag("RijVijComponents");
  
  typedef std::pair<const mapKey, mapdata> mappair;

  BOOST_FOREACH(const mappair& pair1, rvdotacc)
    {
      XML << xmlw::tag("Element")
	  << xmlw::attr("Type") 
	  << pair1.first.first
	  << xmlw::attr("EventName") 
	  << getName(pair1.first.second, Sim);

      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("Rij")
	      << xmlw::attr("dimension")
	      << iDim
	      << xmlw::chardata();

	  pair1.second.rij[iDim].outputHistogram(XML, 1.0);

	  XML << xmlw::endtag("Rij");
	}

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("Vij")
	      << xmlw::attr("dimension")
	      << iDim
	      << xmlw::chardata();

	  pair1.second.vij[iDim].outputHistogram(XML, 1.0);

	  XML << xmlw::endtag("Vij");
	}

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("RijVijvsRij")
	      << xmlw::attr("dimension")
	      << iDim
	      << xmlw::chardata();

	  for (size_t i(0); i < 2000; ++i)
	    XML << ((i - 1000.0) / 1000.0) << " "
		<< pair1.second.rijcostheta[iDim][i].second 
	      / pair1.second.rijcostheta[iDim][i].first
		<< "\n";

	  XML << xmlw::endtag("RijVijvsRij");
	}

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("RijvsRijVij")
	      << xmlw::attr("dimension")
	      << iDim
	      << xmlw::chardata();

	  for (size_t i(0); i < 1000; ++i)
	    XML << ( static_cast<Iflt>(i) / -1000.0) << " "
		<< pair1.second.costhetarij[iDim][i].second 
	      / pair1.second.costhetarij[iDim][i].first
		<< "\n";

	  XML << xmlw::endtag("RijvsRijVij");
	}

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("XijRvdot")
	      << xmlw::attr("dimension")
	      << iDim
	      << xmlw::chardata();

	  for (size_t i1(0); i1 < 200; ++i1)
	    {	      
	      for (size_t i2(0); i2 < 100; ++i2)
		XML << ( (static_cast<Iflt>(i1) - 100.0) / 100.0) << " "
		    << ( static_cast<Iflt>(i2) / -100.0) << " "
		    << static_cast<Iflt>(pair1.second.anglemap[iDim][i1][i2])
		  / static_cast<Iflt>(pair1.second.anglemapcount)
		    << "\n";

	      XML << "\n";
	    }
	  

	  XML << xmlw::endtag("XijRvdot");
	}
      
      XML << xmlw::endtag("Element");
    }
  
  
    XML << xmlw::endtag("RijVijComponents");
}
