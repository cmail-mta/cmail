var cmail = cmail || {};

cmail.mail = {

	get_all: function() {
		var self = this;

		var obj = {};

		if (gui.elem("unread_checkbox").checked) {
			obj["mail_only_unread"] = true;
		}

		ajax.asyncPost(cmail.api_url + "mailbox/?get", JSON.stringify(obj), function(xhr) {
			var mailbox = JSON.parse(xhr.response);

			if (mailbox.status != "ok" && mailbox != "warning") {
				cmail.set_status(mailbox.status);
				return;
			}

			if (mailbox.status == "warning") {
				cmail.set_status(mailbox.warning);
			}

			var body = gui.elem("mail_list");
			body.innerHTML = "";
			mailbox.mails.forEach(function(mail) {
				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(mail["mail_id"]));
				tr.appendChild(gui.createColumn(mail["mail_read"]));
				if (mail["mail_date"]) {
					tr.appendChild(gui.createColumn(mail["mail_date"]));
				} else {
					tr.appendChild(gui.createColumn(mail["mail_submission"]));
				}
				tr.appendChild(gui.createColumn(mail["mail_envelopefrom"]));
				tr.appendChild(gui.createColumn(mail["mail_envelopeto"]));
				tr.appendChild(gui.createColumn(mail["mail_subject"]));

				//options
				var options = gui.create("td");
				options.classList.add("align_left");
				options.appendChild(gui.createButton("read", self.read, [mail], self));
				options.appendChild(gui.createButton("delete", self.delete, [mail], self));

				var buttonName = "Mark as read";
				if (mail["mail_read"] == 1) {
					buttonName = "Unread";
				}
				
				options.appendChild(gui.createButton(buttonName, self.toggleread, [mail], self));
				tr.appendChild(options);

				body.appendChild(tr);
			});
		});
	},
	delete: function(mail) {
		var self = this;
		if (confirm("Do you really delete this mail?") == true) {
			ajax.asyncPost(cmail.api_url + "mailbox/?delete", JSON.stringify(mail), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		}
	},
	toggleread: function(mail) {
		var self = this;

		var url = "mailbox/?read";
		if (mail["mail_read"] == 1) {
			url = "mailbox/?unread";
		}

		ajax.asyncPost(cmail.api_url + url, JSON.stringify(mail), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.get_all();
		});
	},
	read: function(mail) {

		var self = this;
		ajax.asyncPost(cmail.api_url + "mailbox/?read", JSON.stringify(mail), function(xhr) {
			var obj = JSON.parse(xhr.response);
			cmail.set_status(obj.status);

			var mail = obj.mail;

			self.currMail = mail;

			var splitted = mail["mail_data"].split("\r\n\r\n");

			gui.elem("mail_id").textContent = mail["mail_id"];
			gui.elem("mail_subject").textContent = mail["mail_subject"];
			gui.elem("mail_from").textContent = mail["mail_from"];
			gui.elem("mail_to").textContent = mail["mail_to"];
			gui.elem("mail_body").innerText = mail["mail_body"];

			cmail.switch_hash("#single");
		});
	},
	unread: function() {
		var self = this;
		ajax.asyncPost(cmail.api_url + "mailbox/?unread", JSON.stringify(self.currMail), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.back();
		});
	},
	back: function() {
		this.get_all();
		window.location.hash = "#list";
		cmail.switch_hash();
	}
}
