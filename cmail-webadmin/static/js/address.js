var cmail = cmail || {};

cmail.address = {
	url: `${cmail.api_url}address/`
	get: function(address, callback) {
		var self = this;
		api.post(`${self.url}?get`, JSON.stringify(address), function(resp) {
			var addresses = resp.addresses;

			callback(addresses[0]);
		});
	},
	get_all: function() {
		var self = this;

		var url = `${self.url}?get`;
		var test = gui.value('#address_test');

		if (test) {
			url += `&test=${test}`;
		}

		api.get(url, function(resp) {

			var addresses = resp.addresses;
			var addresslist = gui.elem('addresslist');
			addresslist.innerHTML = '';
			var last_address = null;
			var next_address = null;
			var address = null;
			var button = null;

			for (var i = 0; i < addresses.length; i++) {

				address = addresses[i];
				last_address = addresses[i - 1];
				next_address = addresses[i + 1];

				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn(address.address_expression));
				tr.appendChild(gui.createColumn(address.address_order));
				tr.appendChild(gui.createColumn(address.address_router));
				tr.appendChild(gui.createColumn(address.address_route));

				var options = gui.create('td');
				if (last_address) {
					options.appendChild(gui.createButton("▲", self.switch_order, [last_address, address], self));
				} else {
					button = gui.createButton("▲", function() {}, [], self);
					button.style.visibility = 'hidden';
					options.appendChild(button);
				}
				if (next_address) {
					options.appendChild(gui.createButton("▼", self.switch_order, [next_address, address], self));
				} else {
					button = gui.createButton("▼", function() {}, [], self);
					button.style.visibility = 'hidden';
					options.appendChild(button);
				}
				options.appendChild(gui.createButton('edit', self.show_form, [address], self));
				options.appendChild(gui.createButton('delete', self.delete, [address], self));

				tr.appendChild(options);
				addresslist.appendChild(tr);
			};
		});

	},
	show_form: function(obj) {
		if (obj) {
			gui.elem('form_address_type').value = 'edit';

			// get the newest entry
			this.get(obj, function(address) {

				if (!address) {
					cmail.set_status('Address not found!');
					return;
				}

				gui.elem('address_expression').value = address.address_expression;
				gui.elem('address_expression').disabled = true;
				gui.elem('address_old_order').value = address.address_order;
				gui.elem('address_order').value = address.address_order;
				gui.elem('address_router').value = address.address_router;
				gui.elem('address_route').value = address.address_route;
				gui.elem('addressadd').style.display = 'block';
			});

		} else {
			gui.elem('form_address_type').value = 'new';
			gui.elem('address_expression').value = '';
			gui.elem('address_expression').disabled = false;
			gui.elem('address_order').value = '';
			gui.elem('address_router').value = '';
			gui.elem('address_route').value = '';
			gui.elem('addressadd').style.display = 'block';
		}

	},
	hide_form: function() {
		gui.elem('addressadd').style.display = 'none';
	},
	delete: function(obj) {
		var self = this;
		if (confirm(`Really delete the address expression ${obj.address_expression} (Order ${obj.address_order})?`) == true) {
			api.post(`${self.url}?delete`, JSON.stringify(obj), function(post) {
				self.get_all();
			});
		}
	},
	save: function() {
		var self = this;
		var address = {
			address_expression: gui.value('#address_expression'),
			address_order: gui.value('#address_order'),
			address_old_order: gui.value('#address_old_order'),
			address_router: gui.value('#address_router'),
			address_route: gui.value('#address_route')
		};

		if (gui.value('#form_address_type') === 'new') {
			api.post(`${self.url}?add`, JSON.stringify(address), function(resp) {
				self.get_all();
			});
		} else {
			api.post(`${self.url}?update`, JSON.stringify(address), function(resp) {
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

		api.post(`${self.url}?switch`, JSON.stringify(obj), function(resp) {
			self.get_all();
		});
	}
};
