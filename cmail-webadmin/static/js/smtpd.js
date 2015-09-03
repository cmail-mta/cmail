var cmail = cmail || {};

cmail.smtpd = {
	get_all: function() {
		var self = this;

		ajax.asyncGet(cmail.api_url + "smtpd/?get", function(xhr) {
			var obj = JSON.parse(xhr.response);

			if (obj.status != "ok" && obj.status != "warning") {
				cmail.set_status(obj.status);
			}

			if (obj.status == "warning") {
				cmail.set_status("WARNING: " + obj.warning);
			}

			var smtpds = obj.smtpd;

			var body = gui.elem("smtpdlist");
			body.innerHTML = "";
			smtpds.forEach(function(smtpd) {

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
		if (confirm("Do you really delete the address " + username + "?") == true) {
			ajax.asyncPost(cmail.api_url + "smtpd/?delete", JSON.stringify({ smtpd_user: username }), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
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
		ajax.asyncPost(cmail.api_url + url, JSON.stringify(smtpd), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
		});
		this.hide_form();
		this.get_all();
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

		ajax.asyncPost(cmail.api_url + "addresses/?test", JSON.stringify(obj), function(xhr) {
			var resp = JSON.parse(xhr.response);

			if (resp.status !== "ok" && resp.status !== "warning") {
				cmail.set_status(resp.status);
				return;
			}

			if (resp.status === "warning") {
				cmail.set_status(resp.warning);
			}

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
