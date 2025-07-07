run: Release/OriginSimulator.app/Contents/MacOS/OriginSimulator
	$<

Release/OriginSimulator.app/Contents/MacOS/OriginSimulator: qmake
	xcodebuild

qmake:
	qmake -spec macx-xcode OriginSimulator.pro

moc:
	for i in moc_CelestronOriginSimulator.cpp moc_CommandHandler.cpp moc_EnhancedMosaicCreator.cpp moc_ProperHipsClient.cpp moc_StatusSender.cpp moc_WebSocketConnection.cpp; do /opt/homebrew/Cellar/qt/6.9.0/share/qt/libexec/moc `echo $$i|sed -e 's=^moc_==' -e 's=.cpp=.h='` -o $$i; done
