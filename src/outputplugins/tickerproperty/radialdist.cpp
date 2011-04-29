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

#include "radialdist.hpp"
#include "../../dynamics/include.hpp"
#include "../../dynamics/liouvillean/liouvillean.hpp"
#include "../../extcode/mathtemplates.hpp"
#include <magnet/xmlwriter.hpp>
#include <magnet/xmlreader.hpp>
#include <boost/foreach.hpp>

OPRadialDistribution::OPRadialDistribution(const dynamo::SimData* tmp, 
					   const magnet::xml::Node& XML):
  OPTicker(tmp,"RadialDistribution"),
  binWidth(1.0),
  length(100),
  sampleCount(0)
{ 
  if (NDIM != 3)
    M_throw() << "This plugin will not work as I've not correctly calculated "
      "the volume of a shell";
  operator<<(XML); 
}

void 
OPRadialDistribution::operator<<(const magnet::xml::Node& XML)
{
  try {
    binWidth = XML.getAttribute("binWidth").as<double>(0.1)
      * Sim->dynamics.units().unitLength();
    
    if (XML.getAttribute("length").valid())
      length = XML.getAttribute("length").as<size_t>();
    else
      {
	size_t mindir = 0;
	for (size_t iDim = 1; iDim < NDIM; ++iDim)
	  if (Sim->primaryCellSize[iDim] > Sim->primaryCellSize[mindir])
	    mindir = iDim;
	
	//Times 2 as the max dist is half a box length
	//+2 for rounding truncation and for a zero bin       
	length = 2 
	  + static_cast<size_t>(Sim->primaryCellSize[mindir] / (2 * binWidth));
      }

    I_cout() << "Binwidth = " << binWidth / Sim->dynamics.units().unitLength()
	     << "\nLength = " << length;
  }
  catch (std::exception& excep)
    {
      M_throw() << "Error while parsing " << name << "options\n"
		<< excep.what();
    }
}

void 
OPRadialDistribution::initialise()
{
  data.resize
    (Sim->dynamics.getSpecies().size(),
     std::vector<std::vector<unsigned long> >
     (Sim->dynamics.getSpecies().size(),
      std::vector<unsigned long>(length, 0))
     );

  ticker();
}

void 
OPRadialDistribution::ticker()
{
  ++sampleCount;
  
  BOOST_FOREACH(const magnet::ClonePtr<Species>& sp1, Sim->dynamics.getSpecies())
    BOOST_FOREACH(const magnet::ClonePtr<Species>& sp2, Sim->dynamics.getSpecies())
    { BOOST_FOREACH(const size_t& p1, *sp1->getRange())
	BOOST_FOREACH(const size_t& p2, *sp2->getRange())
	{
	  Vector  rij = Sim->particleList[p1].getPosition()
	    - Sim->particleList[p2].getPosition();

	  Sim->dynamics.BCs().applyBC(rij);

	  size_t i = (long) (((rij.nrm())/binWidth) + 0.5);

	  if (i < length)
	      ++data[sp1->getID()][sp2->getID()][i];
	}}
}

void
OPRadialDistribution::output(xml::XmlStream& XML)
{
  XML << xml::tag("RadialDistribution")
      << xml::attr("SampleCount")
      << sampleCount;

  
  BOOST_FOREACH(const magnet::ClonePtr<Species>& sp1, Sim->dynamics.getSpecies())
    BOOST_FOREACH(const magnet::ClonePtr<Species>& sp2, Sim->dynamics.getSpecies())
    {
      double density = sp2->getCount() / Sim->dynamics.getSimVolume();

      unsigned long originsTaken = sampleCount * sp1->getCount();

      XML << xml::tag("Species")
	  << xml::attr("Name1")
	  << sp1->getName()
      	  << xml::attr("Name2")
	  << sp2->getName()
	  << xml::chardata();
      

      //Skip zero as it is wrong due to particle self-self correaltions
      //when sp1==sp2
      for (size_t i = 1; i < length; ++i)
	{
	  double radius = binWidth * i;
	  double volshell = (4.0 * M_PI * binWidth * radius * radius) +
	    ((M_PI * binWidth * binWidth * binWidth) / 3.0);
	  double GR = static_cast<double>(data[sp1->getID()][sp2->getID()][i])
	    / (density * originsTaken * volshell);

	  XML << radius / Sim->dynamics.units().unitLength() << " " 
	      << GR << "\n";
	}


      XML << xml::endtag("Species");
    }
  
  XML << xml::endtag("RadialDistribution");

  I_cout() << "Be warned, if a bin spans a hard core "
    "\n(E.g a bin width of 0.1 will span an interaction diameter of 1 at bin"
    "\n number 10 [bin r=(10 +- 0.5)*binwidth])"
    "\nYou will find a reduced value of g(r) there. You must renormalise by"
    "\nthe difference in the shell volumes, for the previous case it is just"
    "\ngr=gr*2, then correct the bin centre by r=r+0.5*binWidth.";
}
