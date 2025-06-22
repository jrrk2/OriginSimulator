qmake:
	qmake -spec macx-xcode OriginSimulator.pro

moc:
	for i in moc_OriginSimulator.cpp; do /opt/homebrew/Cellar/qt/6.9.0/share/qt/libexec/moc `echo $$i|sed -e 's=^moc_==' -e 's=.cpp=.cpp='` -o build/moc/$$i; done

