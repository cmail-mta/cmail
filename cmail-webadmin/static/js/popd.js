
cmail.pop = {

	get_all: function() {
		var self = this;
		ajax.asyncGet(cmail.api_url + "pop/?get", function(xhr) {
			var pop = JSON.parse(xhr.response);

			if (pop.status != "ok" && pop.status != "warning") {
				cmail.set_status(pop.status);
				return;
			}

			if (pop.status == "warning") {
				cmail.set_status("WARNING: " + pop.warning);
			}

			var list = gui.elem("pop_user_list");
			list.innerHTML = "";
			pop.pop.forEach(function(p) {

				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(p.pop_user));

				var lock = gui.create("td");
				var checkbox = gui.createCheckbox("popLock", function() {
					if(confirm("Really unlock the POP mutex for this user?\nMultiple concurrent accesses to a POP3 account may have undesired consequences.")) {
						ajax.asyncPost(cmail.api_url + "pop/?unlock", JSON.stringify({
							pop_user: p.pop_user
						}), function(xhr) {
							cmail.set_status(pop.status);
							self.get_all();
						});
					} else {
						checkbox.checked = p.pop_lock;
					}
				});
				lock.appendChild(checkbox);
				tr.appendChild(lock);

				var options = gui.create("td");

				options.appendChild(gui.createButton("delete", self.delete, [p], self));

				tr.appendChild(options);

				list.appendChild(tr);
			});
		});
	},
	add: function() {
		var self = this;

		var obj = {
			pop_user: gui.elem("pop_user").value
		};

		ajax.asyncPost(cmail.api_url + "pop/?add", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	},
	delete: function(p) {
		var self = this;

		if (confirm("Really revoke POP3 access from this user?\nPlease note that this will not remove any mail and the user will still be able to receive mail.")) {

			ajax.asyncPost(cmail.api_url + "pop/?delete", JSON.stringify(p), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
	}
}
