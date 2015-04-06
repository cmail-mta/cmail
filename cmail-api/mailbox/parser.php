<?php

class MailParser {

	private $mail;
	private $headers;
	private $body;
	private $attachments;
	private $error;


	private function convertEncodedWord($str) {

		// [0] = charset
		// [1] = encoding (Q or B)
		// [2] = text
		$split = explode("?", $str);

		switch (strtolower($split[1])) {
			case "b":
				$decoded = base64_decode($split[2]);
				break;
			case "q":
				$decoded = quoted_printable_decode($split[2]);
				break;
			default:
				$decoded = $str;
				error_log("No valid encoding");
				break;	
		}

		return mb_convert_encoding($decoded, "UTF-8", $split[0]);

	}
	private function replaceEncodedWords($line) {
			
		$index = strpos($line, "=?");
		if ($index === FALSE) {
			return $line;
		}

		$index_end = strpos($line, "?=");
	
		if ($index_end === FALSE) {
			return $line;
		}

		$output = substr($line, 0, $index);
		$output .= $this->convertEncodedWord(substr($line, $index + 2, $index_end - $index - 2));
		$end = substr($line, $index_end + 2);


		$count = 0;

		// if long encoded long word
		
		while ($end[$count] == " ") {

			$count++;
		}
		if (strpos("=?", $end) == $count) {
			$end = substr($end, $count);
		}

		$output .= $end;

		return $this->replaceEncodedWords($output);
	}

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
			$header_body = $this->replaceEncodedWords(substr($line, $valueCount));
			$this->headers[substr($line, 0, $index)] = $header_body;
		}

		return true;
	}


	function extractBoundary($ct) {
		$index = strpos($ct, "boundary=");

		if ($index === FALSE) {
			$this->body = $raw;
			return;
		}
		$boundary = substr($ct, $index + 9);

		if ($boundary[0] == "\"") {
			$boundary = substr($boundary, 1);
		}

		$index = strpos($boundary, "\"");

		if ($index !== FALSE) {
			$boundary = substr($boundary, 0, $index);
		}

		return $boundary;
	}

	function parseMultipartSignedBody($ct, $raw) {
		//TODO: implement parse multipart signed
		//

		$boundary = $this->extractBoundary($ct);
		error_log("found boundary: " . $boundary);



		$this->body = $raw;	
	}

	function parseMultipartUnsignedBody($ct, $raw) {
		//TODO: implement parse multipart unsigned
	}

	function parseTextPlainBody($ct, $raw) {
		//TODO: implement encoding check
		
		$this->body = $raw;
	}

	function parseBody($raw) {

		$contentType = $this->getHeader("content-type");

		$splitted = explode(";", $contentType);

		if (count($splitted) < 1) {
			$this->body = $raw;
			return;
		}

		error_log($splitted[0]);

		if ($splitted[0][0] == " ") {
			$splitted[0] = substr($splitted[0], 1);
		}

		switch ($splitted[0]) {

			case "text/plain":
				$this->parseTextPlainBody($contentType, $raw);
				break;
			case "multipart/signed":
				$this->parseMultipartSignedBody($contentType, $raw);
				break;
			case "multipart/unsigned":
				$this->parseMultipartUnsignedBody($contentType, $raw);
				break;
			default:
				$this->body = $raw;
				break;
		}
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
			$this->parseBody(substr($mail, $index + 4));
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
