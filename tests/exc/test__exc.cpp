#include <libmary/libmary.h>


using namespace M;


int main (void)
{
    libMaryInit ();

    errs->print ("Hello, World!\n");

    NativeFile file ("my_test_file", 0 /* open_flags */, File::AccessMode::WriteOnly);
    if (exc)
#if 0
	debug_g (grp_all)
	info_g (grp_all)

	debug ()
	info ()
	warn ()
	error ()
	fail ()
#endif
	logs->print ("Open: ").print (exc->toString ()->mem ()).print ("\n");
//	debug ("Open: ", exc, "\n");

    if (!file.print ("TEST OUTPUT\n"))
	logs->print ("Print: ").print (exc->toString ()->mem ()).print ("\n");

    if (!file.close ())
	logs->print ("Close: ").print (exc->toString ()->mem ()).print ("\n");

    return 0;
}

