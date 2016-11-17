cmail = cmail || {};
cmail.delegate = {
	url: `${cmail.api_url}delegates/`,
	get_all: function() {
		var self = this;
		api.get(`${self.url}?get`, function(resp) {

			var userslist = gui.elem('delegates_user');
			var addresseslist = gui.elem('delegates_address');

			userslist.innerHTML = '';
			addresseslist.innerHTML = '';
			resp.delegates['users'].forEach(function(user) {

				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn(user.api_user));
				tr.appendChild(gui.createColumn(user.api_delegate));

				var options = gui.create('td');
				var deleteButton = gui.createButton('delete', self.delete_user, [user], self);
				deleteButton.classList.add('admin');
				options.appendChild(deleteButton);

				tr.appendChild(options);
				userslist.appendChild(tr);
			});

			resp.delegates['addresses'].forEach(function(address) {
				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn(address.api_user));
				tr.appendChild(gui.createColumn(address.api_expression));

				var options = gui.create('td');
				var deleteButton = gui.createButton('delete', self.delete_address, [address], self);
				deleteButton.classList.add('admin');
				options.appendChild(deleteButton);

				tr.appendChild(options);
				addresseslist.appendChild(tr);
			});
		});

	},
	add_user: function() {
		var self = this;

		var obj = {
			api_user: gui.value('#delegate_user_add'),
			api_delegate: gui.value('#delegate_delegation_add')
		};

		api.post(`${self.url}?user_add`, JSON.stringify(obj), function(resp) {
			self.get_all();
		});
	},
	add_address: function() {
		var self = this;

		var obj = {
			api_user: gui.value('#delegate_user_add'),
			api_expression: gui.value('#delegate_delegation_add')
		};

		api.post(`${self.url}?address_add`, JSON.stringify(obj), function(resp) {
			self.get_all();
		});
	},
	delete_user: function(p) {
		var self = this;

		if (confirm('Do you really want to revoke access to this user delegation?')) {
			api.post(`${self.url}?user_delete`, JSON.stringify(p), function(resp) {
				self.get_all();
			});
		}
	},
	delete_address: function(p) {
		var self = this;

		if (confirm('Do you really want to revoke access to this address delegation?')) {
			api.post(`${self.url}?address_delete`, JSON.stringify(p), function(resp) {
				self.get_all();
			});
		}
	}
};
