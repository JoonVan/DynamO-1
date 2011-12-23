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

#include <coil/RenderObj/Light.hpp>
#include <coil/glprimatives/arrow.hpp>
#include <coil/RenderObj/console.hpp>
#include <magnet/gtk/numericEntry.hpp>
#include <magnet/clamp.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>

extern const guint8 Light_Icon[];
extern const size_t Light_Icon_size;

namespace coil {
  Glib::RefPtr<Gdk::Pixbuf> 
  RLight::getIcon()
  {
    return Gdk::Pixbuf::create_from_inline(Light_Icon_size, Light_Icon);
  }

  void 
  RLight::deinit()
  {
  }

  void 
  RLight::init(const std::tr1::shared_ptr<magnet::thread::TaskQueue>& systemQueue) 
  {
    RenderObj::init(systemQueue);
    initGTK();
  }

  void
  RLight::initGTK()
  {
    _optList.reset(new Gtk::VBox);

    { //Intensity
      Gtk::HBox* box = manage(new Gtk::HBox);
      box->show();
      
      {
	Gtk::Label* label = manage(new Gtk::Label("Intensity", 0.95, 0.5));
	box->pack_start(*label, true, true); 
	label->show();
      }

      _intensityEntry.reset(new Gtk::Entry);
      box->pack_start(*_intensityEntry, false, false);
      _intensityEntry->show(); _intensityEntry->set_width_chars(7);
      _intensityEntry->set_text(boost::lexical_cast<std::string>(_intensity));

      _intensityEntry->signal_changed()
	.connect(sigc::bind<Gtk::Entry&>(&magnet::gtk::forceNumericEntry, *_intensityEntry));
      _intensityEntry->signal_activate().connect(sigc::mem_fun(*this, &RLight::guiUpdate));

      {
	Gtk::Label* label = manage(new Gtk::Label("Attenuation", 0.95, 0.5));
	box->pack_start(*label, true, true);
	label->show();
      }

      _attenuationEntry.reset(new Gtk::Entry);
      box->pack_start(*_attenuationEntry, false, false);
      _attenuationEntry->show(); _attenuationEntry->set_width_chars(7);
      _attenuationEntry->set_text(boost::lexical_cast<std::string>(_attenuation));

      _attenuationEntry->signal_changed()
	.connect(sigc::bind<Gtk::Entry&>(&magnet::gtk::forceNumericEntry, *_attenuationEntry));
      _attenuationEntry->signal_activate().connect(sigc::mem_fun(*this, &RLight::guiUpdate));

      _optList->pack_start(*box, false, false); 
    }

    { //Specular
      Gtk::HBox* box = manage(new Gtk::HBox);
      box->show();

      {
	Gtk::Label* label = manage(new Gtk::Label("Specular Exponent", 0.95, 0.5));
	box->pack_start(*label, true, true); 
	label->show();
      }

      _specularExponentEntry.reset(new Gtk::Entry);
      box->pack_start(*_specularExponentEntry, false, false);
      _specularExponentEntry->show(); _specularExponentEntry->set_width_chars(7);
      _specularExponentEntry->set_text(boost::lexical_cast<std::string>(_specularExponent));

      _specularExponentEntry->signal_changed()
	.connect(sigc::bind<Gtk::Entry&>(&magnet::gtk::forceNumericEntry, *_specularExponentEntry));
      _specularExponentEntry->signal_activate().connect(sigc::mem_fun(*this, &RLight::guiUpdate));

      {
	Gtk::Label* label = manage(new Gtk::Label("Specular Strength", 0.95, 0.5));
	box->pack_start(*label, true, true); 
	label->show();
      }

      _specularFactorEntry.reset(new Gtk::Entry);
      box->pack_start(*_specularFactorEntry, false, false);
      _specularFactorEntry->show(); _specularFactorEntry->set_width_chars(7);
      _specularFactorEntry->set_text(boost::lexical_cast<std::string>(_specularFactor));

      _specularFactorEntry->signal_changed()
	.connect(sigc::bind<Gtk::Entry&>(&magnet::gtk::forceNumericEntry, *_specularFactorEntry));
      _specularFactorEntry->signal_activate().connect(sigc::mem_fun(*this, &RLight::guiUpdate));

      _optList->pack_start(*box, false, false);
    }

    _optList->show();

    guiUpdate();
  }

  void
  RLight::showControls(Gtk::ScrolledWindow* win)
  {
    win->remove();
    _optList->unparent();
    win->add(*_optList);
    win->show();
  }

  void 
  RLight::guiUpdate()
  {
    try { _intensity = boost::lexical_cast<float>(_intensityEntry->get_text()); } catch (...) {}
    try { _attenuation = boost::lexical_cast<float>(_attenuationEntry->get_text()); } catch (...) {}
    try { _specularExponent = boost::lexical_cast<float>(_specularExponentEntry->get_text()); } catch (...) {}
    try { _specularFactor = boost::lexical_cast<float>(_specularFactorEntry->get_text()); } catch (...) {}
  }
}
