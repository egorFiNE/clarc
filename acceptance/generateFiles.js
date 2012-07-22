var fs = require('fs');

var targetPath = process.argv[2];
var count = parseInt(process.argv[3]);
var maxSize = parseInt(process.argv[4]);

if (!targetPath || !count || !maxSize) {
	console.log("Usage: generateFiles.js path count maxsize");
	process.exit(0);
}

var randomBuffer = new Buffer(maxSize+1);

var i;
for (i=0;i<count;i++) {
	generateRandomFile(randomBuffer, targetPath, maxSize);
}

function generateRandomFile(buffer, path, maxSize) {
	var fileLength = Math.floor(Math.random() * maxSize)+1;
	var newBuffer = buffer.slice(0, fileLength);
	var fileName = path+'/'+generateRandomString()+'.bin';
	fs.writeFileSync(fileName, newBuffer);
	console.log(fileName);
}

function generateRandomString() {
	var text = "";
	var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	var i;

	var len = Math.floor(Math.random() * 12)+1;
	for(i=0;i<len;i++) {
		text += possible.charAt(Math.floor(Math.random() * possible.length));
	}

	return text;
}

