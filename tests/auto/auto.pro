TEMPLATE = aux

OTHER_FILES += \
    ../*.xml \
    *.qml

unittest.files = *.qml
unittest.path = /opt/tests/sociald/auto

shared.files = ../../src/eventfeed/*.qml
shared.path = /opt/tests/sociald/auto/eventfeed/shared

twitter.files = ../../src/twitter/twitter-eventfeed/*.qml
twitter.path = /opt/tests/sociald/auto/eventfeed

INSTALLS += unittest shared twitter
