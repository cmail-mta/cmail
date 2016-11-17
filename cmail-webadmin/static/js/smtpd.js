var cmail = cmail || {};

cmail.smtpd = {
	url: `${cmail.api_url}smtpd/`,
	get_all: function() {
		var self = this;

		api.get(`${self.url}?get`, function(resp) {

			var body = gui.elem('smtpdlist');
			body.innerHTML = '';
			resp.smtpd.forEach(function(smtpd) {

				var tr = gui.create('tr');

				tr.appendChild(gui.createColumn(smtpd['smtpd_user']));
				tr.appendChild(gui.createColumn(smtpd['smtpd_router']));
				tr.appendChild(gui.createColumn(smtpd['smtpd_route']));

				var options = gui.create('td');

				options.appendChild(gui.createButton('edit', self.show_form, [smtpd['smtpd_user']], self));
				options.appendChild(gui.createButton('delete', self.delete, [smtpd['smtpd_user']], self));

				tr.appendChild(options);

				body.appendChild(tr);
			});
		});
	},
	get: function(username) {
		return new Promise(function(resolve, reject) {
			api.post(`${self.url}?get`, JSON.stringify({ smtpd_user: username }), function(resp) {
				if (obj.smtpd.length > 0) {
					resolve(obj.smtpd[0]);
					return;
				}

				cmail.set_status('No user found.');
				reject();
			});
		});
	},
	delete: function(username) {
		var self = this;
		if (confirm(`Really remove ${username} from the SMTPD ACL?\nThe user will no longer be able to originate mail.\nPlease note that the user will still be able to receive mail.`)) {
			api.post(`${self.url}?delete`, JSON.stringify({ smtpd_user: username }), function(resp) {
				self.get_all();
			});
		}

	},
	show_form: function(username) {
		var self = this;
		if (username) {
			gui.elem('form_smtpd_type').value = 'edit';
			self.get(username).then(function (smtpd) {
				gui.elem('smtpd_user').value = smtpd.smtpd_user;
				gui.elem('smtpd_user').disabled = true;
				gui.elem('smtpd_router').value = smtpd.smtpd_router;
				gui.elem('smtpd_route').value = smtpd.smtpd_route;
				gui.elem('smtpdadd').style.display = 'block';
			});
		} else {
			gui.elem('form_smtpd_type').value = 'new';
			gui.elem('smtpd_user').value = '';
			gui.elem('smtpd_router').value = 'drop';
			gui.elem('smtpd_route').value = '';
			gui.elem('smtpdadd').style.display = 'block';
		}
	},
	hide_form: function() {
		gui.elem('smtpdadd').style.display = 'none';
	},
	save: function() {
		var smtpd = {
			smtpd_user: gui.value('#smtpd_user'),
			smtpd_router: gui.value('#smtpd_router'),
			smtpd_route: gui.value('#smtpd_route')
		};
		var url = '';
		if (gui.value('#form_smtpd_type') === 'new') {
			url = '?add';
		} else {
			url = '?update';
		}
		api.post(self.url + url, JSON.stringify(smtpd), function(resp) {
			this.hide_form();
			this.get_all();
		});
	},
	test: function() {
		var self = this;
		var address = gui.value('#smtpd_test_input');
		var router = gui.value('#smtpd_test_router');

		if (!address) {
			cmail.set_status("Mail address is empty.");
			return;
		}

		var obj = {
			address_expression: address,
			address_routing: router
		}
		api.post(`${self.url}?test`, JSON.stringify(obj), function(resp) {

			var body = gui.elem('smtpd_test_steps');
			body.innerHTML = '';
			resp.steps.forEach(function(step, i) {
				var tr = gui.create('tr');

				tr.appendChild(gui.createColumn(i));
				tr.appendChild(gui.createColumn(step));

				body.appendChild(tr);
			});
		});
	}
};
