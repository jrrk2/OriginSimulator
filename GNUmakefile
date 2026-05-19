QT5_QMAKE=/opt/homebrew/opt/qt@5/bin/qmake

all: Release/OriginSimulator.app/Contents/MacOS/OriginSimulator

run: Release/OriginSimulator.app/Contents/MacOS/OriginSimulator
	$<

Release/OriginSimulator.app/Contents/MacOS/OriginSimulator: qmake
	xcodebuild CODE_SIGN_IDENTITY="-" CODE_SIGNING_ALLOWED=NO

qmake:
	$(QT5_QMAKE) -spec macx-xcode OriginSimulator.pro

moc:
	for i in moc_CelestronOriginSimulator.cpp moc_CommandHandler.cpp moc_EnhancedMosaicCreator.cpp moc_ProperHipsClient.cpp moc_StatusSender.cpp moc_WebSocketConnection.cpp moc_GaiaStarFieldRenderer.cpp moc_SimulatorMainWindow.cpp; do /opt/homebrew/opt/qt@5/bin/moc `echo $$i|sed -e 's=^moc_==' -e 's=.cpp=.h='` -o $$i; done

clean:
	rm -rf debug release .xcode build Debug Release OriginSimulator.xcodeproj OriginSimulator.app
