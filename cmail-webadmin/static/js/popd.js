
cmail.pop = {

	get_all: function() {
		var self = this;
		api.get(cmail.api_url + "pop/?get", function(resp) {

			var list = gui.elem("pop_user_list");
			list.innerHTML = "";
			resp.pop.forEach(function(p) {
				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(p.pop_user));

				var lock = gui.create("td");
				var checkbox = gui.createCheckbox("popLock", function() {
					if(confirm("Really unlock the POP mutex for this user?\nMultiple concurrent accesses to a POP3 account may have undesired consequences.")) {
						api.post(cmail.api_url + "pop/?unlock", JSON.stringify({
							pop_user: p.pop_user
						}), function(resp) {
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

		api.post(cmail.api_url + "pop/?add", JSON.stringify(obj), function(resp) {
			self.get_all();
		});
	},
	delete: function(p) {
		var self = this;

		if (confirm("Really revoke POP3 access from this user?\nPlease note that this will not remove any mail and the user will still be able to receive mail.")) {

			api.post(cmail.api_url + "pop/?delete", JSON.stringify(p), function(resp) {
				self.get_all();
			});
		}
	}
}
