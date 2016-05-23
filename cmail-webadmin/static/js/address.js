var cmail = cmail || {};

cmail.address = {
	get: function(address) {
		var xhr = ajax.syncPost(cmail.api_url + "addresses/?get", JSON.stringify(address));
		var addresses = JSON.parse(xhr.response).addresses;

		return addresses[0];
	},
	test: function() {
		var test = document.getElementById("address_test").value;

		this.get_all(test);
	},
	get_all: function(test) {
		var self = this;

		var url = cmail.api_url + "addresses/?get";

		if (test) {
			url += "&test=" + test;
		}

		ajax.asyncGet(url, function(xhr) {

			var obj  = JSON.parse(xhr.response);

			if (obj.status != "ok" && obj.status != "warning") {
				cmail.set_status(obj.status);
				return;
			}

			if (obj.status == "warning") {
				cmail.set_status(obj.warning);
			}

			var addresses = obj.addresses;
			var addresslist = gui.elem("addresslist");
			addresslist.innerHTML = "";
			var last_address = null;
			var next_address = null;
			var address = null;
			var button = null;

			for (var i = 0; i < addresses.length; i++) {

				address = addresses[i];
				last_address = addresses[i - 1];
				next_address = addresses[i + 1];

				var tr = gui.create("tr");
				tr.appendChild(gui.createColumn(address.address_expression));
				tr.appendChild(gui.createColumn(address.address_order));
				tr.appendChild(gui.createColumn(address.address_router));
				tr.appendChild(gui.createColumn(address.address_route));

				var options = gui.create("td");
				if (last_address) {
					options.appendChild(gui.createButton("▲", self.switch_order, [last_address, address], self));
				} else {
					button = gui.createButton("▲", function() {}, [], self);
					button.style.visibility = "hidden";
					options.appendChild(button);
				}
				if (next_address) {
					options.appendChild(gui.createButton("▼", self.switch_order, [next_address, address], self));
				} else {
					button = gui.createButton("▼", function() {}, [], self);
					button.style.visibility = "hidden";
					options.appendChild(button);
				}
				options.appendChild(gui.createButton("edit", self.show_form, [address], self));
				options.appendChild(gui.createButton("delete", self.delete, [address], self));

				tr.appendChild(options);
				addresslist.appendChild(tr);
			};
		});

	},
	show_form: function(obj) {
		if (obj) {
			gui.elem("form_address_type").value = "edit";

			// get the newest entry
			var address = this.get(obj);

			if (!address) {
				cmail.set_status("Address not found!");
				return;
			}

			gui.elem("address_expression").value = address.address_expression;
			gui.elem("address_expression").disabled = true;
			gui.elem("address_order").value = address.address_order;
			gui.elem("address_router").value = address.address_router;
			gui.elem("address_route").value = address.address_route;
		} else {
			gui.elem("form_address_type").value = "new";
			gui.elem("address_expression").value = "";
			gui.elem("address_expression").disabled = false;
			gui.elem("address_order").value = "";
			gui.elem("address_router").value = "";
			gui.elem("address_route").value = "";
		}

		gui.elem("addressadd").style.display = "block";
	},
	hide_form: function() {
		gui.elem("addressadd").style.display = "none";
	},
	delete: function(obj) {
		var self = this;
		if (confirm("Really delete the address expression " + obj.address_expression + " (Order " + obj.address_order + ")?") == true) {
			ajax.asyncPost(cmail.api_url + "addresses/?delete", JSON.stringify(obj), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
	},
	save: function() {
		var self = this;
		var address = {
			address_expression: gui.elem("address_expression").value,
			address_order: gui.elem("address_order").value,
			address_router: gui.elem("address_router").value,
			address_route: gui.elem("address_route").value
		};

		if (gui.elem("form_address_type").value === "new") {
			ajax.asyncPost(cmail.api_url + "addresses/?add", JSON.stringify(address), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		} else {
			ajax.asyncPost(cmail.api_url + "addresses/?update", JSON.stringify(address), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
		this.hide_form();
	},
	switch_order: function(address1, address2) {

		var obj = {
			address1: address1,
			address2: address2
		};
		var self = this;

		ajax.asyncPost(cmail.api_url + "addresses/?switch", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	},

};
