# TOAST environment variables generated with configure
# Run this with 'source toastenv.csh' from your csh/tcsh shell.

if ( $?TOASTDIR ) then
else
    echo "TOASTDIR environment variable not set!"
    if ( -e mtoast_install.m ) then
	setenv TOASTDIR $PWD
    endif
endif

if ( $?TOASTDIR ) then
    echo "Using TOASTDIR = $TOASTDIR"
    setenv TOASTVER $TOASTDIR/linux64
    setenv TOAST_SCRIPT_PATH $TOASTDIR/script

# Add $TOASTVER/bin to path
    set EXEPATH = ($TOASTVER/bin)
    echo ${path} | egrep -i "$EXEPATH" >& /dev/null
    if ($status != 0) then
	set path = ( $EXEPATH $path )
    endif

# Add $TOASTVER/lib to library path
    set LIBPATH = ($TOASTVER/lib)
    if ( $?LD_LIBRARY_PATH ) then
	echo ${LD_LIBRARY_PATH} | egrep -i "$LIBPATH" >& /dev/null
	if ($status != 0) then
	    setenv LD_LIBRARY_PATH "$LIBPATH":"$LD_LIBRARY_PATH"
	endif
    else
	setenv LD_LIBRARY_PATH $LIBPATH
    endif

    echo "TOAST environment has been set."
else
    echo "Cannot set the TOAST environment"
endif

