/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef __LIBMARY__CONNECTION__H__
#define __LIBMARY__CONNECTION__H__


#include <libmary/libmary_config.h>
#include <libmary/types.h>
#include <libmary/cb.h>
#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>
#include <libmary/util_net.h>


namespace M {

class Connection : public AsyncInputStream,
		   public AsyncOutputStream
{
public:
  // TODO Consider moving InputFrontend and OutputFrontend
  //      to AsyncInputStream and AsyncOutputStream respectively.
  //      Another option would be to abolsh those clasess
  //      as excessive abstractions.

    struct InputFrontend {
	void (*processInput) (void *cb_data);

	void (*processError) (Exception *exc_,
			      void      *cb_data);
    };

    struct OutputFrontend {
	void (*processOutput) (void *cb_data);
    };

protected:
    Cb<InputFrontend> input_frontend;
    Cb<OutputFrontend> output_frontend;

public:
    // TODO Варианты:
    //      fin ();   // (частичный shutdown - если понадобится)
    //      close (); // <- Не асинхронный. Асинхронным занимаается Sender.
    //                // НО нужно помнить о SO_LINGER, хотя он здесь и не нужен.
    virtual mt_throws Result close () = 0;

#ifdef LIBMARY_ENABLE_MWRITEV
    virtual int getFd () = 0;
#endif

    void setInputFrontend (Cb<InputFrontend> const &input_frontend)
    {
	this->input_frontend = input_frontend;
    }

    void setOutputFrontend (Cb<OutputFrontend> const &output_frontend)
    {
	this->output_frontend = output_frontend;
    }
};

}


#endif /* __LIBMARY__CONNECTION__H__ */

