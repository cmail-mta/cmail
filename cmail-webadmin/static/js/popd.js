
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
		ajax.asyncPost(cmail.api_url + "pop/?delete", JSON.stringify(p), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	}
}
