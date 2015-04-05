<?php

class MailParser {

	private $mail;
	private $headers;
	private $body;
	private $attachments;
	private $error;

	private function parseHeaders($raw) {

		$lines = explode("\r\n", $raw);

		$count = 0;

		$unfolded = array();
		foreach ($lines as $line) {

			if ($line == "") {
				break;
			}
			
			if ($line[0] == " " || $line[0] == "\t") {
				$unfolded[$count] .= substr($line, 1);	
			} else {
				$count++;
				$unfolded[$count] = $line;	
			}
		}

		foreach ($unfolded as $line) {
			$index = strpos($line, ":");

			if ($index === FALSE) {
				$error = "Error parsing header line: " . $line;
				return false;
			}

			$valueCount = $index + 1;
			if ($line[$index + 1] != " ") {
				$valueCount++;
			}
			$this->headers[substr($line, 0, $index)] = substr($line, $valueCount);
		}

		return true;
	}

	function parseBody($raw) {

		$this->body = $raw;
	}

	public function __construct($mail) {

		$this->mail = $mail;
		$this->headers = array();
		$this->body = "";
		$this->attachments = array();
		$this->error = "ok";

		$index = strpos($mail,"\r\n\r\n");
		if ($index === FALSE) {
			$this->parseHeaders($mail);
		} else {
			$this->parseHeaders(substr($mail, 0, $index));
			$this->parseBody(substr($mail, $index + 2));
		}
	}

	public function getHeader($header) {
		
		foreach ($this->headers as $tag => $value) {

			if (strtolower($tag) === $header) {
				return $value;
			}
		}
	}

	public function getHeaders() {
		return $this->headers;
	}

	public function getBody() {
		return $this->body;
	}

	public function getAttachments() {
		return $this->atttachments;
	}
}

?>
