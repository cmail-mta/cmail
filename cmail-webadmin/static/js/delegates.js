
cmail.delegates = {

	get_all: function() {
		var self = this;
		ajax.asyncGet(cmail.api_url + "delegates/?get", function(xhr) {
			var obj = JSON.parse(xhr.response);

			if (obj.status != "ok") {
				cmail.set_status(obj.status);
				if (obj.status != "warning") {
					return;
				}
			}

			var userslist = gui.elem("delegates_user");
			var addresseslist = gui.elem("delegates_address");

			userslist.innerHTML = "";
			addresseslist.innerHTML = "";
			obj.delegates["users"].forEach(function(user) {

				var tr = gui.create("tr");
				tr.appendChild(gui.createColumn(user.api_user));
				tr.appendChild(gui.createColumn(user.api_delegate));

				var options = gui.create("td");
				var deleteButton = gui.createButton("delete", self.delete_user, [user], self);
				deleteButton.classList.add("admin");
				options.appendChild(deleteButton);

				tr.appendChild(options);
				userslist.appendChild(tr);
			});

			obj.delegates["addresses"].forEach(function(address) {
				var tr = gui.create("tr");
				tr.appendChild(gui.createColumn(address.api_user));
				tr.appendChild(gui.createColumn(address.api_expression));

				var options = gui.create("td");
				var deleteButton = gui.createButton("delete", self.delete_address, [address], self);
				deleteButton.classList.add("admin");
				options.appendChild(deleteButton);

				tr.appendChild(options);
				addresseslist.appendChild(tr);
			});
		});

	},
	add_user: function() {
		var self = this;

		var obj = {
			api_user: gui.elem("delegate_user_add").value,
			api_delegate: gui.elem("delegate_delegation_add").value
		};

		ajax.asyncPost(cmail.api_url + "delegates/?user_add", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	},
	add_address: function() {
		var self = this;

		var obj = {
			api_user: gui.elem("delegate_user_add").value,
			api_expression: gui.elem("delegate_delegation_add").value
		};

		ajax.asyncPost(cmail.api_url + "delegates/?address_add", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	},
	delete_user: function(p) {
		var self = this;

		if (confirm("Do you really want to revoke access to this user delegation?")) {

			ajax.asyncPost(cmail.api_url + "delegates/?user_delete", JSON.stringify(p), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
	},
	delete_address: function(p) {
		var self = this;

		if (confirm("Do you really want to revoke access to this address delegation?")) {

			ajax.asyncPost(cmail.api_url + "delegates/?address_delete", JSON.stringify(p), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
	}
}
