var nodetk = require('../index.js');

function showInfoBox(message, title="Information") {
	nodetk.eval(`tk_messageBox -icon info -type ok -message "${message}" -title "${title}"`);
}

function setText(widgetId, content) {
	nodetk.setVar("_tmpTextContent", content);
	nodetk.eval(`
		${widgetId} delete 0.0 end
		${widgetId} insert end $_tmpTextContent
	`)
}

// Button click event callback
function onButtonClick(text) {
	console.log("The button was clicked");
	console.log("text=", text);
	showInfoBox("The following text was entered: '" + text + "'");
}

function main() {

	// Global error handling
	process.on('uncaughtException', function (err) {
		if (err instanceof nodetk.TclError) {
			console.error("TCL Error:", err);
		}
		else {
			console.error(err);
		}
	});

	nodetk.init();

	nodetk.createCommand("onButtonClick", onButtonClick);

	nodetk.eval(`
		pack [text .myText -width 70 -height 10]
		pack [button .myButton1 -text "Button" -command {onButtonClick [.myText get 1.0 {end -1c}]}]
	`);

	setText(".myText", `Node version: ${process.version}\nCurrent directory: ${process.cwd()}\nPlease enter some text: ...`);

	// Gui Loop
	const guiLoopId = setInterval(function() {
		// handle all events
		while (nodetk.doOneEvent() > 0) {
			// ...
		}
	 	// Progam, finished
		if (nodetk.getNumMainWindows() == 0) {
			clearInterval(guiLoopId);
			nodetk.finit();
			console.log("Program stopped!");
		}
	}, 0);
}

main();