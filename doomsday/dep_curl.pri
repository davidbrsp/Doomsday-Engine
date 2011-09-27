# Build configuration for cURL.
win32 {
    isEmpty(CURL_DIR) {
        error("dep_curl: cURL SDK path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $$CURL_DIR/include
    LIBS += -l$$CURL_DIR/libcurl
    
    # Install the libcurl shared library.
    INSTALLS += curllibs
    curllibs.files = $$CURL_DIR/libcurl.dll 
    curllibs.path = $$DENG_WIN_PRODUCTS_DIR
}
else:macx {
    LIBS += -lcurl
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libcurl --cflags)
    LIBS += $$system(pkg-config libcurl --libs)
}
