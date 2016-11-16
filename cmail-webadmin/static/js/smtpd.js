var cmail = cmail || {};

cmail.smtpd = {
	get_all: function() {
		var self = this;

		api.get(cmail.api_url + "smtpd/?get", function(resp) {

			var body = gui.elem("smtpdlist");
			body.innerHTML = "";
			resp.smtpd.forEach(function(smtpd) {

				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(smtpd["smtpd_user"]));
				tr.appendChild(gui.createColumn(smtpd["smtpd_router"]));
				tr.appendChild(gui.createColumn(smtpd["smtpd_route"]));

				var options = gui.create("td");

				options.appendChild(gui.createButton("edit", self.show_form, [smtpd["smtpd_user"]], self));
				options.appendChild(gui.createButton("delete", self.delete, [smtpd["smtpd_user"]], self));

				tr.appendChild(options);

				body.appendChild(tr);
			});
		});
	},
	get: function(username) {
		var xhr = ajax.syncPost(cmail.api_url + "smtpd/?get", JSON.stringify({ smtpd_user: username }));

		var obj = JSON.parse(xhr.response);
		if (obj.status != "ok" && obj.status != "warning") {
			cmail.set_status(obj.status);
			return false;
		}

		if (obj.status == "warning") {
			cmail.set_status(obj.warning);
		}

		if (obj.smtpd.length > 0) {
			return obj.smtpd[0];
		}

		cmail.set_status("No user found.");
		return false;
	},
	delete: function(username) {
		var self = this;
		if (confirm("Really remove " + username + " from the SMTPD ACL?\nThe user will no longer be able to originate mail.\nPlease note that the user will still be able to receive mail.") == true) {
			api.post(cmail.api_url + "smtpd/?delete", JSON.stringify({ smtpd_user: username }), function(resp) {
				self.get_all();
			});
		}

	},
	show_form: function(username) {
		if (username) {
			gui.elem("form_smtpd_type").value = "edit";
			var smtpd = this.get(username);

			if (smtpd === false) {
				return;
			}

			gui.elem("smtpd_user").value = smtpd.smtpd_user;
			gui.elem("smtpd_user").disabled = true;
			gui.elem("smtpd_router").value = smtpd.smtpd_router;
			gui.elem("smtpd_route").value = smtpd.smtpd_route;

		} else {
			gui.elem("form_smtpd_type").value = "new";

			gui.elem("smtpd_user").value = "";
			gui.elem("smtpd_router").value = "drop";
			gui.elem("smtpd_route").value = "";
		}

		gui.elem("smtpdadd").style.display = "block";
	},
	hide_form: function() {
		gui.elem("smtpdadd").style.display = "none";
	},
	save: function() {
		var smtpd = {
			smtpd_user: gui.elem("smtpd_user").value,
			smtpd_router: gui.elem("smtpd_router").value,
			smtpd_route: gui.elem("smtpd_route").value
		};
		var url = "";
		if (gui.elem("form_smtpd_type").value == "new") {
			url = "smtpd/?add";
		} else {
			url = "smtpd/?update";
		}
		api.post(cmail.api_url + url, JSON.stringify(smtpd), function(resp) {
			this.hide_form();
			this.get_all();
		});
	},
	test: function() {
		var address = gui.elem("smtpd_test_input").value;
		var router = gui.elem("smtpd_test_router").value;

		if (address === "") {
			cmail.set_status("Mail address is empty.");
			return;
		}

		var obj = {
			address_expression: address,
			address_routing: router
		}
		api.post(cmail.api_url + "addresses/?test", JSON.stringify(obj), function(resp) {

			var body = gui.elem("smtpd_test_steps");
			body.innerHTML = "";
			resp.steps.forEach(function(step, i) {
				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(i));
				tr.appendChild(gui.createColumn(step));

				body.appendChild(tr);
			});
		});
	}
};
