/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
"use strict";
var ajax = ajax || {
	contentType: "application/json",
	accept: "application/json",
	handleRequest(type, url, success, fail, user, pass, payload, async) {
		payload = payload || null;
		async = async || true;
		var request = new XMLHttpRequest();
		request.onreadystatechange = function() {
			if (request.readyState === 4) {
				if (request.status / 100 === 2) {
					success(request);
				} else {
					fail(request);
				}
			}
		}

		try {
			request.open(type, url, async, user, pass);
			request.setRequestHeader("Content-type", this.contentType);
			request.setRequestHeader("Accept", this.accept);
			request.send(payload);
		} catch(e) {
			fail(request);
		}
	},

	/**
	 * Make an asynchronous GET request
	 * Calls /readyfunc/ with the request as parameter upon completion (readyState == 4)
	 */
	asyncGet: function(url, user, pass) {
		var self = this;
		return new Promise(function(resolve, reject) {
			self.handleRequest('GET', url, resolve, reject, user, pass, null, true);
		});
	},
	/**
	  Make an asynchronous POST request
	  Calls /readyfunc/ with the request as parameter upon completion (readyState == 4)

	  /payload/ should contain the data to be POSTed in the format specified by contentType,
	  by defualt form-urlencoded
	  */
	asyncPost: function(url, payload, user, pass, contentType) {
		var self = this;
		return new Promise(function(resolve, reject) {
			self.handleRequest('POST', url, resolve, reject, user, pass, payload, true);
		});
	},
	asyncPut: function(url, payload, user, pass, contentType) {
		var self = this;
		return new Promise(function(resolve, reject) {
			self.handleRequest('PUT', url, resolve, reject, user, pass, payload, true);
		});
	},
	/**
	 * Perform a synchronous GET request
	 * This function does not do any error checking, so exceptions might
	 * be thrown.
	 * @depricated
	 */
	syncGet: function(url, user, pass) {
		var request = new this.ajaxRequest();
		request.open("GET", url, false, user, pass);
		request.send(null);
		return request;
	},
	/**
	 * Perform a synchronous POST request, with /payload/
	 * being the data to POST in the specified format (default: application/json)
	 * @depricated
	 */
	syncPost: function(url, payload, user, pass) {
		var request = new this.ajaxRequest();
		request.open("POST", url, false, user, pass);
		request.setRequestHeader("Content-type", this.contentType);
		request.send(payload);
		return request;
	}
};

var api = api || {
	errorfunc: function(xhr) {
		if (!xhr.response) {
			cmail.set_status('No response received.');
			return;
		}
		cmail.set_status(xhr.statusText);
	},
	success: function(xhr, successfunc) {
		successfunc = successfunc || function() {};
		if (xhr.status === 204) {
			successfunc({content: []});
			return;
		}

		var response = {};
		try {
			response = JSON.parse(xhr.response);
		} catch (e) {
			cmail.set_status('Cannot parse JSON.');
			console.log(e);
		}

		cmail.set_status(xhr.statusText);

		successfunc(response);
	},
	get: function(url, successfunc) {
		var self = this;
		ajax.asyncGet(url).then(function(xhr) {
			self.success(xhr, successfunc);
		}, this.errorfunc);
	},
	post: function(url, payload, successfunc) {
		var self = this;
		ajax.asyncPost(url, payload).then(function(xhr) {
			self.success(xhr, successfunc);
		}, this.errorfunc);
	},
	put: function(url, payload, successfunc) {
		var self = this;
		ajax.asyncPut(url, payload).then(function(xhr) {
			self.success(xhr, successfunc);
		}, this.errorfunc);
	}
}

var gui = gui || {
	elem: function(elem) {
		return document.getElementById(elem);
	},
	create: function(tag) {
		return document.createElement(tag);
	},
	createOption: function(text, value) {
		var option = document.createElement("option");
		option.setAttribute("value", value);
		option.textContent = text;

		return option;
	},
	createCheckbox: function(styleClass, clickEvent) {
		var elem = gui.create("input");
		elem.setAttribute("class", styleClass);
		elem.setAttribute("type","checkbox");
		elem.addEventListener("click", clickEvent);

		return elem;
	},
	/**
	 * creates a span object with the given content
	 * @param {String} the content of the span
	 * @returns {Node span}
	 */
	createText: function(content) {
		var text = gui.create('span');
		text.textContent = content;
		return text;
	},
	createColumn: function(content, id) {
		var col = this.create('td');
		col.setAttribute('id', id);
		col.textContent = content;
		return col;
	},
	createButton: function(text, func, param, caller) {
		var button = gui.create('span');
		button.classList.add('button');
		button.textContent = text;
		button.addEventListener('click', function() {
			if (caller) {
				func.apply(caller, param);
			} else {
				func(param);
			}
		});
		return button;
	}
};

var tools = tools || {
};
