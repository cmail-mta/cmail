var cmail = cmail || {};

cmail.user = {
	url: `${cmail.api_url}users/`,
	user_permissions: [],
	/**
	 * Returns a single user object from api
	 * @param name name of the user
	 * @return user object
	 */
	get: function(name) {
		var self = this;
		return new Promise(function(resolve, reject) {
			api.post(`self.url}?get`, JSON.stringify({ username: name}), function(resp) {
				if (resp.users.length < 1) {
					reject();
				}

				resolve(resp.users[0]);
			});
		});
	},
	/**
	 * Gets all users from api and fills the user table.
	 */
	get_all: function() {
		var self = this;
		api.get(`${self.url}?get`, function(resp) {

			// set users
			var users = resp.users;
			self.users = users;

			// table element
			var userlist = gui.elem('userlist');
			userlist.innerHTML = '';

			// userlist
			var list = {};
			users.forEach(function(user) {
				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn(user.user_name));
				tr.appendChild(gui.createColumn(user.user_alias));
				tr.appendChild(gui.createColumn(user.link_count));

				cmail.modules.forEach(function(module) {
					var col = gui.create('td');
					var cb = gui.createCheckbox(`${module}_cb`);
					if (user.modules[module]) {
						cb.checked = true;
					}
					cb.disabled = true;
					col.appendChild(cb);
					tr.appendChild(col);
				});

				tr.appendChild(gui.createColumn(user.mails));

				// option buttons
				var options = gui.create('td');
				options.appendChild(gui.createButton('edit', self.show_form, [user.user_name], self));
				options.appendChild(gui.createButton('delete', self.delete, [user.user_name], self));
				tr.appendChild(options);
				userlist.appendChild(tr);

				list[user.user_name] = user.user_name;
			});

			self.fill_username_list(list);
			self.setPermissions(resp.permissions);
		});

	},
	setPermissions: function(permissions) {
		if(!permissions['admin']) {
			gui.addStylesheet('static/admin.css');
			return;
		}

		if (!permissions['delegate'] && !permissions['admin']) {
			gui.addStylesheet('static/delegate.css');
			return;
		}
	},
	fill_username_list: function(list) {
		var userlist = gui.elem('usernamelist');
		userlist.innerHTML = '';
		Object.keys(list).forEach(function(username) {
			userlist.appendChild(gui.createOption('', username));
		});
	},
	delete_permission: function(tr, permission) {
		var index = this.user_permissions.indexOf(permission);

		if (index < 0) {
			return;
		}

		this.user_permissions.splice(index, 1);
		gui.elem('user_permissions').removeChild(tr);
	},
	add_permission: function() {
		var permission = gui.value('#user_permission');
		this.appendPermission(permission);
	},
	appendPermission: function(permission) {

		var self = this;
		this.user_permissions.push(permission);
		var permissions = gui.elem('user_permissions');
		var tr = gui.create('tr');
		tr.appendChild(gui.createColumn(permission));
		var option = gui.create('td');
		option.appendChild(gui.createButton('delete', self.delete_permission, [tr, permission], self));
		tr.appendChild(option);
		permissions.appendChild(tr);
	},
	show_form: function(name) {
		var self = this;
		this.user_permissions = [];
		gui.elem('user_permission').innerHTML = '';
		if (name) {
			gui.elem('form_type').value = 'edit';
			self.get(name).then(function(user) {

				gui.elem('user_name').value = user.user_name;
				gui.elem('user_name').disabled = true;

				gui.elem('user_alias').value = user.user_alias;

				user_permissions = gui.elem('user_permissions');
				user_permissions.innerHTML = '';
				user.user_permissions.forEach(function(permission) {
					self.appendPermission(permission);
				});
				gui.elem('useradd').style.display = 'block';
			});

		} else {

			gui.elem('form_type').value = 'new';
			gui.elem('user_name').value = '';
			gui.elem('user_name').disabled = false;
			gui.elem('user_alias').value = '';
			gui.elem('user_permissions').innerHTML = '';
			gui.elem('useradd').style.display = 'block';

		}

		gui.elem('user_password').value = '';
		gui.elem('user_password2').value = '';
		gui.elem('user_password_revoke').checked = false;
	},
	hide_form: function() {
		gui.elem('useradd').style.display = 'none';
	},
	delete: function(name) {
		var self = this;
		if (confirm("Do you really want to delete this user?\nThis step cannot be undone.\nAll mail stored for this user will be deleted.")) {
			api.post(`${self.url}?delete`, JSON.stringify({ user_name: name }), function(resp){
				self.get_all();
			});
		}
	},
	save: function() {
		var authdata = null;
		var self = this;

		if (gui.value('#user_password') !== gui.value('#user_password2')) {
			cmail.set_status('Password is not the same');
			return;
		}

		var authdata = gui.value('#user_password');

		if (!authdata) {
			authdata = null;
		}

		var user = {
			user_name: gui.value('#user_name'),
			user_authdata: authdata,
			user_permissions: self.user_permissions
		};

		if (gui.value('#form_type') === 'new') {

			if (gui.value('#user_alias')) {
				user['user_alias'] = gui.value('#user_alias');
			}

			api.post(`${self.url}?add`, JSON.stringify(user));
		} else {
			if (gui.elem('user_password_revoke').checked) {
				authdata = true;
				user['user_authdata'] = null;
			}
			if (authdata) {
				api.post(`${self.url}?set_password`, JSON.stringify(user));
			}

			if (!gui.elem('user_alias')) {
				alias = null;
			} else {
				alias = gui.value('#user_alias');
			}

			api.post(`${self.url}?update_alias`, JSON.stringify({
				user_name: user.user_name,
				user_alias: alias
			}));

			api.post(`${self.url}?update_permissions`, JSON.stringify({
				user_name: user.user_name,
				user_permissions: self.user_permissions
			}), function(resp) {
				self.get_all();
			});

		}
		this.hide_form();
	}
};
