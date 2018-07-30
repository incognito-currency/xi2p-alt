QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = i2pd_qt
TEMPLATE = app
QMAKE_CXXFLAGS *= -std=c++11 -ggdb
DEFINES += USE_UPNP

# change to your own path, where you will store all needed libraries with 'git clone' commands below.
MAIN_PATH = /path/to/libraries

# git clone https://github.com/PurpleI2P/Boost-for-Android-Prebuilt.git
# git clone https://github.com/PurpleI2P/OpenSSL-for-Android-Prebuilt.git
# git clone https://github.com/PurpleI2P/MiniUPnP-for-Android-Prebuilt.git
# git clone https://github.com/PurpleI2P/android-ifaddrs.git
BOOST_PATH = $$MAIN_PATH/Boost-for-Android-Prebuilt
OPENSSL_PATH = $$MAIN_PATH/OpenSSL-for-Android-Prebuilt
MINIUPNP_PATH = $$MAIN_PATH/MiniUPnP-for-Android-Prebuilt
IFADDRS_PATH = $$MAIN_PATH/android-ifaddrs

# Steps in Android SDK manager:
# 1) Check Extras/Google Support Library https://developer.android.com/topic/libraries/support-library/setup.html
# 2) Check API 11
# Finally, click Install.

SOURCES += DaemonQT.cpp mainwindow.cpp \
    ../../libxi2p/api.cpp \
    ../../libxi2p/Base.cpp \
    ../../libxi2p/BloomFilter.cpp \
    ../../libxi2p/Config.cpp \
    ../../libxi2p/CPU.cpp \
    ../../libxi2p/Crypto.cpp \
	../../libxi2p/CryptoKey.cpp \
    ../../libxi2p/Datagram.cpp \
    ../../libxi2p/Destination.cpp \
    ../../libxi2p/Event.cpp \
    ../../libxi2p/Family.cpp \
    ../../libxi2p/FS.cpp \
    ../../libxi2p/Garlic.cpp \
    ../../libxi2p/Gost.cpp \
    ../../libxi2p/Gzip.cpp \
    ../../libxi2p/HTTP.cpp \
    ../../libxi2p/I2NPProtocol.cpp \
    ../../libxi2p/I2PEndian.cpp \
    ../../libxi2p/Identity.cpp \
    ../../libxi2p/LeaseSet.cpp \
    ../../libxi2p/Log.cpp \
    ../../libxi2p/NetDb.cpp \
    ../../libxi2p/NetDbRequests.cpp \
    ../../libxi2p/NTCPSession.cpp \
    ../../libxi2p/Profiling.cpp \
    ../../libxi2p/Reseed.cpp \
    ../../libxi2p/RouterContext.cpp \
    ../../libxi2p/RouterInfo.cpp \
    ../../libxi2p/Signature.cpp \
    ../../libxi2p/SSU.cpp \
    ../../libxi2p/SSUData.cpp \
    ../../libxi2p/SSUSession.cpp \
    ../../libxi2p/Streaming.cpp \
    ../../libxi2p/Timestamp.cpp \
    ../../libxi2p/TransitTunnel.cpp \
    ../../libxi2p/Transports.cpp \
    ../../libxi2p/Tunnel.cpp \
    ../../libxi2p/TunnelEndpoint.cpp \
    ../../libxi2p/TunnelGateway.cpp \
    ../../libxi2p/TunnelPool.cpp \
    ../../libxi2p/util.cpp \
    ../../libxi2p/Ed25519.cpp \
    ../../libxi2p/Chacha20.cpp \
    ../../libxi2p/Poly1305.cpp \    
    ../../libxi2p_client/AddressBook.cpp \
    ../../libxi2p_client/BOB.cpp \
    ../../libxi2p_client/ClientContext.cpp \
    ../../libxi2p_client/HTTPProxy.cpp \
    ../../libxi2p_client/I2CP.cpp \
    ../../libxi2p_client/I2PService.cpp \
    ../../libxi2p_client/I2PTunnel.cpp \
    ../../libxi2p_client/MatchedDestination.cpp \
    ../../libxi2p_client/SAM.cpp \
    ../../libxi2p_client/SOCKS.cpp \
    ../../libxi2p_client/Websocket.cpp \
    ../../libxi2p_client/WebSocks.cpp \
    ClientTunnelPane.cpp \
    MainWindowItems.cpp \
    ServerTunnelPane.cpp \
    SignatureTypeComboboxFactory.cpp \
    TunnelConfig.cpp \
    TunnelPane.cpp \
    ../../daemon/Daemon.cpp \
    ../../daemon/HTTPServer.cpp \
    ../../daemon/i2pd.cpp \
    ../../daemon/I2PControl.cpp \
    ../../daemon/UnixDaemon.cpp \
    ../../daemon/UPnP.cpp \
    textbrowsertweaked1.cpp \
    pagewithbackbutton.cpp \
    widgetlock.cpp \
    widgetlockregistry.cpp \
    logviewermanager.cpp

#qt creator does not handle this well
#SOURCES += $$files(../../libxi2p/*.cpp)
#SOURCES += $$files(../../libxi2p_client/*.cpp)
#SOURCES += $$files(../../daemon/*.cpp)
#SOURCES += $$files(./*.cpp)

SOURCES -= ../../daemon/UnixDaemon.cpp

HEADERS  += DaemonQT.h mainwindow.h \
    ../../libxi2p/api.h \
    ../../libxi2p/Base.h \
    ../../libxi2p/BloomFilter.h \
    ../../libxi2p/Config.h \
    ../../libxi2p/Crypto.h \
	../../libxi2p/CryptoKey.h \
    ../../libxi2p/Datagram.h \
    ../../libxi2p/Destination.h \
    ../../libxi2p/Event.h \
    ../../libxi2p/Family.h \
    ../../libxi2p/FS.h \
    ../../libxi2p/Garlic.h \
    ../../libxi2p/Gost.h \
    ../../libxi2p/Gzip.h \
    ../../libxi2p/HTTP.h \
    ../../libxi2p/I2NPProtocol.h \
    ../../libxi2p/I2PEndian.h \
    ../../libxi2p/Identity.h \
    ../../libxi2p/LeaseSet.h \
    ../../libxi2p/LittleBigEndian.h \
    ../../libxi2p/Log.h \
    ../../libxi2p/NetDb.hpp \
    ../../libxi2p/NetDbRequests.h \
    ../../libxi2p/NTCPSession.h \
    ../../libxi2p/Profiling.h \
    ../../libxi2p/Queue.h \
    ../../libxi2p/Reseed.h \
    ../../libxi2p/RouterContext.h \
    ../../libxi2p/RouterInfo.h \
    ../../libxi2p/Signature.h \
    ../../libxi2p/SSU.h \
    ../../libxi2p/SSUData.h \
    ../../libxi2p/SSUSession.h \
    ../../libxi2p/Streaming.h \
    ../../libxi2p/Tag.h \
    ../../libxi2p/Timestamp.h \
    ../../libxi2p/TransitTunnel.h \
    ../../libxi2p/Transports.h \
    ../../libxi2p/TransportSession.h \
    ../../libxi2p/Tunnel.h \
    ../../libxi2p/TunnelBase.h \
    ../../libxi2p/TunnelConfig.h \
    ../../libxi2p/TunnelEndpoint.h \
    ../../libxi2p/TunnelGateway.h \
    ../../libxi2p/TunnelPool.h \
    ../../libxi2p/util.h \
    ../../libxi2p/version.h \
    ../../libxi2p_client/AddressBook.h \
    ../../libxi2p_client/BOB.h \
    ../../libxi2p_client/ClientContext.h \
    ../../libxi2p_client/HTTPProxy.h \
    ../../libxi2p_client/I2CP.h \
    ../../libxi2p_client/I2PService.h \
    ../../libxi2p_client/I2PTunnel.h \
    ../../libxi2p_client/MatchedDestination.h \
    ../../libxi2p_client/SAM.h \
    ../../libxi2p_client/SOCKS.h \
    ../../libxi2p_client/Websocket.h \
    ../../libxi2p_client/WebSocks.h \
    ClientTunnelPane.h \
    MainWindowItems.h \
    ServerTunnelPane.h \
    SignatureTypeComboboxFactory.h \
    TunnelConfig.h \
    TunnelPane.h \
    TunnelsPageUpdateListener.h \
    ../../daemon/Daemon.h \
    ../../daemon/HTTPServer.h \
    ../../daemon/I2PControl.h \
    ../../daemon/UPnP.h \
    textbrowsertweaked1.h \
    pagewithbackbutton.h \
    widgetlock.h \
    widgetlockregistry.h \
    i2pd.rc \
    i2pd.rc \
    logviewermanager.h

INCLUDEPATH += ../../libxi2p
INCLUDEPATH += ../../libxi2p_client
INCLUDEPATH += ../../daemon
INCLUDEPATH += .

FORMS += mainwindow.ui \
    tunnelform.ui \
    statusbuttons.ui \
    routercommandswidget.ui \
    generalsettingswidget.ui

LIBS += -lz

macx {
	message("using mac os x target")
	BREWROOT=/usr/local
	BOOSTROOT=$$BREWROOT/opt/boost
	SSLROOT=$$BREWROOT/opt/libressl
	UPNPROOT=$$BREWROOT/opt/miniupnpc
	INCLUDEPATH += $$BOOSTROOT/include
	INCLUDEPATH += $$SSLROOT/include
	INCLUDEPATH += $$UPNPROOT/include
	LIBS += $$SSLROOT/lib/libcrypto.a
	LIBS += $$SSLROOT/lib/libssl.a
	LIBS += $$BOOSTROOT/lib/libboost_system.a
	LIBS += $$BOOSTROOT/lib/libboost_date_time.a
	LIBS += $$BOOSTROOT/lib/libboost_filesystem.a
	LIBS += $$BOOSTROOT/lib/libboost_program_options.a
	LIBS += $$UPNPROOT/lib/libminiupnpc.a
}

android {
	message("Using Android settings")
        DEFINES += ANDROID=1
	DEFINES += __ANDROID__

        CONFIG += mobility

        MOBILITY =

        INCLUDEPATH += $$BOOST_PATH/boost_1_53_0/include \
		$$OPENSSL_PATH/openssl-1.0.2/include \
		$$MINIUPNP_PATH/miniupnp-2.0/include \
		$$IFADDRS_PATH
	DISTFILES += android/AndroidManifest.xml

	ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

	SOURCES += $$IFADDRS_PATH/ifaddrs.c
	HEADERS += $$IFADDRS_PATH/ifaddrs.h

	equals(ANDROID_TARGET_ARCH, armeabi-v7a){
		DEFINES += ANDROID_ARM7A
		# http://stackoverflow.com/a/30235934/529442
		LIBS += -L$$BOOST_PATH/boost_1_53_0/armeabi-v7a/lib \
			-lboost_system-gcc-mt-1_53 -lboost_date_time-gcc-mt-1_53 \
			-lboost_filesystem-gcc-mt-1_53 -lboost_program_options-gcc-mt-1_53 \
			-L$$OPENSSL_PATH/openssl-1.0.2/armeabi-v7a/lib/ -lcrypto -lssl \
			-L$$MINIUPNP_PATH/miniupnp-2.0/armeabi-v7a/lib/ -lminiupnpc

		PRE_TARGETDEPS += $$OPENSSL_PATH/openssl-1.0.2/armeabi-v7a/lib/libcrypto.a \
			$$OPENSSL_PATH/openssl-1.0.2/armeabi-v7a/lib/libssl.a
		DEPENDPATH += $$OPENSSL_PATH/openssl-1.0.2/include

		ANDROID_EXTRA_LIBS += $$OPENSSL_PATH/openssl-1.0.2/armeabi-v7a/lib/libcrypto_1_0_0.so \
			$$OPENSSL_PATH/openssl-1.0.2/armeabi-v7a/lib/libssl_1_0_0.so \
			$$MINIUPNP_PATH/miniupnp-2.0/armeabi-v7a/lib/libminiupnpc.so
	}

	equals(ANDROID_TARGET_ARCH, x86){
		# http://stackoverflow.com/a/30235934/529442
		LIBS += -L$$BOOST_PATH/boost_1_53_0/x86/lib \
			-lboost_system-gcc-mt-1_53 -lboost_date_time-gcc-mt-1_53 \
			-lboost_filesystem-gcc-mt-1_53 -lboost_program_options-gcc-mt-1_53 \
			-L$$OPENSSL_PATH/openssl-1.0.2/x86/lib/ -lcrypto -lssl \
			-L$$MINIUPNP_PATH/miniupnp-2.0/x86/lib/ -lminiupnpc

		PRE_TARGETDEPS += $$OPENSSL_PATH/openssl-1.0.2/x86/lib/libcrypto.a \
			$$OPENSSL_PATH/openssl-1.0.2/x86/lib/libssl.a

		DEPENDPATH += $$OPENSSL_PATH/openssl-1.0.2/include

		ANDROID_EXTRA_LIBS += $$OPENSSL_PATH/openssl-1.0.2/x86/lib/libcrypto_1_0_0.so \
			$$OPENSSL_PATH/openssl-1.0.2/x86/lib/libssl_1_0_0.so \
			$$MINIUPNP_PATH/miniupnp-2.0/x86/lib/libminiupnpc.so
	}
}

linux:!android {
        message("Using Linux settings")
        LIBS += -lcrypto -lssl -lboost_system -lboost_date_time -lboost_filesystem -lboost_program_options -lpthread -lminiupnpc
}

windows {
        message("Using Windows settings")
        RC_FILE = i2pd.rc
        DEFINES += BOOST_USE_WINDOWS_H WINDOWS _WINDOWS WIN32_LEAN_AND_MEAN MINIUPNP_STATICLIB
        DEFINES -= UNICODE _UNICODE
        BOOST_SUFFIX = -mt
        QMAKE_CXXFLAGS_RELEASE = -Os
        QMAKE_LFLAGS = -Wl,-Bstatic -static-libgcc -static-libstdc++ -mwindows

        #linker's -s means "strip"
        QMAKE_LFLAGS_RELEASE += -s

        LIBS = -lminiupnpc \
        -lboost_system$$BOOST_SUFFIX \
        -lboost_date_time$$BOOST_SUFFIX \
        -lboost_filesystem$$BOOST_SUFFIX \
        -lboost_program_options$$BOOST_SUFFIX \
        -lssl \
        -lcrypto \
        -lz \
        -lwsock32 \
        -lws2_32 \
        -lgdi32 \
        -liphlpapi \
        -lstdc++ \
        -lpthread
}

!android:!symbian:!maemo5:!simulator {
	message("Build with a system tray icon")
	# see also http://doc.qt.io/qt-4.8/qt-desktop-systray-systray-pro.html for example on wince*
	#sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS i2pd_qt.pro resources images
	RESOURCES = i2pd.qrc
	QT += xml
	#INSTALLS += sources
}

