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

#include "particle.hpp"
#include <magnet/xmlwriter.hpp>
#include <magnet/xmlreader.hpp>
#include "../datatypes/vector.xml.hpp"
#include <ostream>

Particle::Particle(const magnet::xml::Node& XML, unsigned long nID):
  _ID(nID),
  _peculiarTime(0.0),
  _state(DEFAULT)
{
  if (XML.getAttribute("Static").valid()) clearState(DYNAMIC);

  _pos << XML.getNode("P");
  _vel << XML.getNode("V");
}

xml::XmlStream& operator<<(xml::XmlStream& XML, 
			    const Particle& particle)
{
  XML << xml::attr("ID") << particle._ID;

  if (!particle.testState(Particle::DYNAMIC))
    XML << xml::attr("Static") <<  "Static";

  XML << xml::tag("P")
      << (particle._pos)
      << xml::endtag("P")
      << xml::tag("V")
      << (particle._vel)
      << xml::endtag("V");
  

  return XML;
}
