var cmail = cmail || {};

cmail.msa = {
	get_all: function() {
		var self = this;

		ajax.asyncGet(cmail.api_url + "msa/?get", function(xhr) {
			var obj = JSON.parse(xhr.response);

			if (obj.status != "ok") {
				cmail.set_status(obj.status);
			}

			var msas = obj.msa;

			var body = gui.elem("msalist");
			body.innerHTML = "";
			msas.forEach(function(msa) {

				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(msa["msa_user"]));
				tr.appendChild(gui.createColumn(msa["msa_inrouter"]));
				tr.appendChild(gui.createColumn(msa["msa_inroute"]));
				tr.appendChild(gui.createColumn(msa["msa_outrouter"]));
				tr.appendChild(gui.createColumn(msa["msa_outroute"]));

				var options = gui.create("td");

				options.appendChild(gui.createButton("edit", self.show_form, [msa["msa_user"]], self));
				options.appendChild(gui.createButton("delete", self.delete, [msa["msa_user"]], self));

				tr.appendChild(options);

				body.appendChild(tr);
			});
		});
	},
	get: function(username) {
		var xhr = ajax.syncPost(cmail.api_url + "msa/?get", JSON.stringify({ msa_user: username }));

		var obj = JSON.parse(xhr.response);
		if (obj.status != "ok") {
			cmail.set_status(obj.status);
			return false;
		}

		if (obj.msa.length > 0) {
			return obj.msa[0];
		}

		cmail.set_status("No user found.");
		return false;
	},
	delete: function(username) {
		var self = this;
		if (confirm("Do you really delete the address " + username + "?") == true) {
			ajax.asyncPost(cmail.api_url + "msa/?delete", JSON.stringify({ msa_user: username }), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get();
			});
		}

	},
	show_form: function(username) {
		if (username) {
			gui.elem("form_msa_type").value = "edit";
			var msa = this.get(username);

			if (msa === false) {
				return;
			}

			gui.elem("msa_user").value = msa.msa_user;
			gui.elem("msa_user").disabled = true;
			gui.elem("msa_inrouter").value = msa.msa_inrouter;
			gui.elem("msa_inroute").value = msa.msa_inroute;
			gui.elem("msa_outrouter").value = msa.msa_outrouter;
			gui.elem("msa_outroute").value = msa.msa_outroute;

		} else {
			gui.elem("form_msa_type").value = "new";

			gui.elem("msa_user").value = "";
			gui.elem("msa_inrouter").value = "store";
			gui.elem("msa_inroute").value = "";
			gui.elem("msa_outrouter").value = "drop";
			gui.elem("msa_outroute").value = "";
		}

		gui.elem("msaadd").style.display = "block";
	},
	hide_form: function() {
		gui.elem("msaadd").style.display = "none";
	},
	save: function() {
		var msa = {
			msa_user: gui.elem("msa_user").value,
			msa_inrouter: gui.elem("msa_inrouter").value,
			msa_inroute: gui.elem("msa_inroute").value,
			msa_outrouter: gui.elem("msa_outrouter").value,
			msa_outroute: gui.elem("msa_outroute").value
		};
		var url = "";
		if (gui.elem("form_msa_type").value == "new") {
			url = "msa/?add";
		} else {
			url = "msa/?update";
		}
		ajax.asyncPost(cmail.api_url + url, JSON.stringify(msa), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
		});
		this.hide_form();
		this.get_all();
	},
	test: function() {
		var address = gui.elem("msa_test_input").value;
		var router = gui.elem("msa_test_router").value;

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

			if (resp.status !== "ok") {
				cmail.set_status(resp.status);
				return;
			}

			var body = gui.elem("msa_test_steps");
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
