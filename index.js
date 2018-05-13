var nodetk = require('bindings')('nodetk');

class TclError extends Error {}

module.exports = nodetk;
module.exports.TclError = TclError;

module.exports.eval = function (script) {
	try {
		return nodetk._eval(script);
	}
	catch (err) {
		throw new TclError(err);
	}
}