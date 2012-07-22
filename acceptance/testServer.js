var 
	http = require('http'),
	fs = require('fs');

var path="";
var allowedFileNames=[];
var redirectedFileNames={};

function isCheckAuth(req, res) {
	if (req.method=="GET" && req.url=="/?location") { // check auth
		if (req.headers.authorization.substr(0,10)=='AWS test1:') { 
			res.writeHead(200, {'Content-Type': 'text/plain'});
			res.end('okay\n'); // clarc only checks http status
			console.log("Auth okay");
		} else { 
			res.writeHead(403, {'Content-Type': 'text/plain'});
			res.end('not okay\n'); // clarc only checks http status
			console.log("Auth failed");
		}
		return true;
	}

	return false;
}

var count=0;

function showProgress(count) {
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



function isPut(req, res) {
	if (req.method=="PUT") {
		if (allowedFileNames.indexOf(req.url)>=0 || redirectedFileNames[req.url]) {
			if (req.url=='/.files.sqlite3') {
				console.log("Database loaded");
			}

			if (redirectedFileNames[req.url]) {
				delete redirectedFileNames[req.url];
				console.log("Got %s, redirected size = %d", req.url, Object.keys(redirectedFileNames).length);
			}

			var randomFileName = generateRandomString();
			if (Math.random()>0.9) {
				console.log("redircted to %s", randomFileName);
				redirectedFileNames['/'+randomFileName] = true;
				res.writeHead(307, {
					'Content-Length': 0,
					'Location': 'http://127.0.0.1/'+randomFileName
				});
				res.end();
			} else {
				showProgress(count++);
				res.writeHead(200, {
					'Content-length': 0,
					'ETag': '"d41d8cd98f00b204e9800998ecf8427e"'
				});
			}
		} else { 
			console.log("ALIEN FILENAME: %s", req.url);
			res.writeHead(403, {
				'Content-length': 0,
			});
		}
		res.end();
		return true;
	}
	return false;
}


path = process.argv[2];
if (!path) {
	console.log("Usage: testServer.js path");
	console.log("path should point to the --source folder.");
	process.exit(1);
}

fs.readdirSync(path).forEach(function(filename) {
	allowedFileNames.push('/'+filename);
});
allowedFileNames.push('/.files.sqlite3');
allowedFileNames.push('/.files.sqlite3-journal');

http.createServer(function (req, res) {
	if (isCheckAuth(req, res)) {
		return;
	}

	if (isPut(req, res)) {
		return;
	}

	res.writeHead(404, {'Content-Type': 'text/plain'});
	res.end("Where?\n");
}).listen(80);

console.log('Server running at http://127.0.0.1:80/');

