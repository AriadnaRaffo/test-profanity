#!/bin/sh

debian_prepare()
{
    echo
    echo Profanity installer ... updating apt repositories
    echo
    sudo apt-get update

    echo
    echo Profanity installer... installing dependencies
    echo
    sudo apt-get -y install git automake autoconf libssl-dev libexpat1-dev libncursesw5-dev libglib2.0-dev libnotify-dev libcurl3-dev libxss-dev

}

fedora_prepare()
{
    echo
    echo Profanity installer... installing dependencies
    echo

    ARCH=`arch`
    
    sudo yum -y install gcc git autoconf automake openssl-devel.$ARCH expat-devel.$ARCH ncurses-devel.$ARCH  glib2-devel.$ARCH libnotify-devel.$ARCH libcurl-devel.$ARCH libXScrnSaver-devel.$ARCH
}

cygwin_prepare()
{
    echo
    echo Profanity installer... installing dependencies
    echo

    wget --no-check-certificate https://raw.github.com/boothj5/apt-cyg/master/apt-cyg
    #wget http://apt-cyg.googlecode.com/svn/trunk/apt-cyg
    chmod +x apt-cyg
    mv apt-cyg /usr/local/bin/

    apt-cyg install make gcc automake autoconf pkg-config openssl-devel libexpat-devel zlib-devel libncurses-devel libncursesw-devel libglib2.0-devel libcurl-devel libidn-devel libssh2-devel libkrb5-devel openldap-devel
    ln -s /usr/bin/gcc-3.exe /usr/bin/gcc.exe

    export LIBRARY_PATH=/usr/local/lib/
}

install_lib_strophe()
{
    echo
    echo Profanity installer... installing libstrophe
    echo
    git clone git://github.com/strophe/libstrophe.git
    cd libstrophe
    ./bootstrap.sh
    ./configure
    make
    sudo make install

    cd ..
}

install_profanity()
{
    echo
    echo Profanity installer... installing Profanity
    echo
    ./configure
    make
    sudo make install
}

cyg_install_lib_strophe()
{
    echo
    echo Profanity installer... installing libstrophe
    echo
    git clone git://github.com/strophe/libstrophe.git
    cd libstrophe
    ./bootstrap.sh
    ./bootstrap.sh # second call seems to fix problem on cygwin
    ./configure
    make
    make install

    cd ..
}

cyg_install_profanity()
{
    echo
    echo Profanity installer... installing Profanity
    echo
    ./configure
    make
    make install
}

cleanup()
{
    echo
    echo Profanity installer... cleaning up
    echo

    echo Removing libstrophe repository...
    rm -rf libstrophe

    echo
    echo Profanity installer... complete!
    echo
    echo Type \'profanity\' to run.
    echo
}

OS=`uname -s`
DIST=unknown

if [ "${OS}" = "Linux" ]; then
    if [ -f /etc/fedora-release ]; then
        DIST=fedora
    elif [ -f /etc/debian_version ]; then
        DIST=debian
    fi
else
    echo $OS | grep -i cygwin
    if [ "$?" -eq 0 ]; then
        DIST=cygwin
    fi
fi

case "$DIST" in
unknown)    echo The install script will not work on this OS.
            echo Try a manual install instead.
            exit
            ;;
fedora)     fedora_prepare
            install_lib_strophe
            install_profanity
            ;;
debian)     debian_prepare
            install_lib_strophe
            install_profanity
            ;;
cygwin)     cygwin_prepare
            cyg_install_lib_strophe
            cyg_install_profanity
            ;;
esac

cleanup
