var cmail = cmail || {};

cmail.address = {
	get: function(expression) {
		var xhr = ajax.syncPost(cmail.api_url + "addresses/?get", JSON.stringify({ address: expression}));
		var addresses = JSON.parse(xhr.response).addresses;

		return addresses[0];
	}, 
	get_all: function() {
		var self = this;

		ajax.asyncGet(cmail.api_url + "addresses/?get", function(xhr) {

			var obj  = JSON.parse(xhr.response);

			if (obj.status != "ok") {
				cmail.set_status(obj.status);
				return;
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
				tr.appendChild(gui.createColumn(address.address_user));

				var options = gui.create("td");
				if (last_address) {
					options.appendChild(gui.createButton("/\\", self.switch_order, [last_address, address], self));
				} else {
					button = gui.createButton("/\\", function() {}, [], self);
					button.style.visibility = "hidden";
					options.appendChild(button);
				}
				if (next_address) {
					options.appendChild(gui.createButton("\\/", self.switch_order, [next_address, address], self));
				} else {
					button = gui.createButton("\\/", function() {}, [], self);
					button.style.visibility = "hidden";
					options.appendChild(button);
				}
				options.appendChild(gui.createButton("edit", self.show_form, [address.address_expression], self));
				options.appendChild(gui.createButton("delete", self.delete, [address.address_expression], self));

				tr.appendChild(options);
				addresslist.appendChild(tr);
			};
		});
	},
	show_form: function(expression) {
		if (expression) {
			gui.elem("form_type").value = "edit";

			var address = this.get(expression);

			if (!address) {
				cmail.set_status("Address not found!");
				return;
			}

			gui.elem("address_expression").value = address.address_expression;
			gui.elem("address_expression").disabled = true;
			gui.elem("address_order").value = address.address_order;
			gui.elem("address_user").value = address.address_user;
		} else {
			gui.elem("form_address_type").value = "new";
			gui.elem("address_expression").value = "";
			gui.elem("address_expression").disabled = false;
			gui.elem("address_order").value = "";
			gui.elem("address_user").value = "";
		}

		gui.elem("addressadd").style.display = "block";
	},
	hide_form: function() {
		gui.elem("addressadd").style.display = "none";
	},
	delete: function(expression) {
		var self = this;
		if (confirm("Do you really delete the address " + expression + "?") == true) {
			ajax.asyncPost(cmail.api_url + "msa/?delete", JSON.stringify({ expression: expression }), function(xhr) {
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
			address_user: gui.elem("address_user").value
		};

		if (gui.elem("form_address_type").value === "new") {
			ajax.asyncPost(cmail.api_url + "addresses/?add", JSON.stringify({address: address}), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		} else {
			ajax.asyncPost(cmail.api_url + "addresses/?update", JSON.stringify({address: address}), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
		this.hide_address_form();
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
