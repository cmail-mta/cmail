<?php
/**
 * This file implements a mail parser.
 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
 */

/**
 * This is the class for parsing mails.
 */
class MailParser {

	/**
	 * the unparsed mail
	 */
	private $mail;
	/**
	 * the headers.
	 */
	private $headers;
	/**
	 * the body of the mail.
	 */
	private $body;
	/**
	 * the attachments of the mail.
	 */
	private $attachments;
	/**
	 * Error messages.
	 */
	private $error;

	/**
	 * encode =?<charset>?<encoding>?<text>?=
	 * =? and ?= is not part of the input string
	 * @param string string to convert.
	 * @return the converted string
	 */
	private function convertEncodedWord($str) {
		// [0] = charset
		// [1] = encoding (Q or B)
		// [2] = text
		$split = explode("?", $str);
		if (count($split) < 3) {
			return "";
		}
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
		$out = mb_convert_encoding($decoded, "UTF-8", $split[0]);
		return $out;
	}

	/*
	 * replace encoded words in a line
	 * @param string $line line to replace the encoded words.
	 * @return string replaced line.
	 */
	private function replaceEncodedWords($line) {
		$index = strpos($line, "=?");
		if ($index === FALSE) {
			return $line;
		}
		// count if enough ? are in the line
		$i = strpos($line, "?", $index + 2);
		$i = strpos($line, "?", $i + 1);

		// end of encoding
		$index_end = strpos($line, "?=", $i + 1);

		if ($index_end === FALSE) {
			return $line;
		}

		$output = substr($line, 0, $index);
		$output .= $this->convertEncodedWord(substr($line, $index + 2, $index_end - $index - 2));
		$end = substr($line, $index_end + 2);

		$output .= $this->replaceEncodedWords($end);
		return $output;
	}

	/**
	 * Parses the headers.
	 * @param string $raw raw header input.
	 * @param string $lineBreak line break to parse.
	 * @return boolean False on error.
	 */
	private function parseHeaders($raw, $lineBreak="\r\n") {

		$lines = explode($lineBreak, $raw);

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
			$tag = substr($line, 0, $index);

			$this->headers[strtolower($tag)] = $header_body;
		}

		return true;
	}

	/*
	 * extract a multipart boundary from content-type header
	 * @param string $ct input for boundary
	 * @return string boundary
	 */
	private function extractBoundary($ct) {
		$index = strpos($ct, "boundary=");

		if ($index === FALSE) {
			$this->body = $raw;
			return null;
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

	/**
	 * Returns if this mail part is a text part
	 * @return boolean True if it is a text part.
	 */
	public function isTextPart() {
		$ct = $this->getHeader("Content-Type");
		// default is a text part
		if ($ct == "") {
			return true;
		}

		$splitted = explode(";", $ct);

		switch(trim($splitted[0])) {
			case 'message/rfc822':
			case 'multipart/mixed':
			case 'text/plain':
				return true;
			default:
				return false;
		}
	}

	/**
	 * Parses the multipart body of a message
	 * @param string $ct Content-Type string
	 * @param string $raw raw mail
	 */
	private function parseMultipartBody($ct, $raw) {

		$boundary = trim($this->extractBoundary($ct));
		$newBody = "";

		// end boundary
		$endBoundary = "--$boundary--";
		$raw2 = explode($endBoundary, $raw);

		$splitted = explode(trim("--$boundary") . $this->newLine, $raw2[0]);
		foreach($splitted as $split) {
			$part = new MailParser($split, $this->showBasicHeaders);
			$this->parts[] = $part;

			if ($part->isTextPart() && trim($part->getBody()) != '') {
				$newBody .= $part->getBody();
				$newBody .= '\n----\n';
			}
		}
		$this->body = $newBody;
	}

	/**
	 * Parses the multipart signed body
	 * @param string $ct Content-Type string
	 * @param string $raw raw mail
	 */
	private function parseMultipartSignedBody($ct, $raw) {
		$this->parseMultipartBody($ct, $raw);
	}

	/**
	 * Parses the multipart unsigned body
	 * @param string $ct Content-Type string
	 * @param string $raw raw mail
	 */
	private function parseMultipartUnsignedBody($ct, $raw) {
		$this->parseMultipartBody($ct, $raw);
	}

	/**
	 * Parses the text plain body
	 * @param string $ct Content-Type string
	 * @param string $raw raw mail.
	 */
	private function parseTextPlainBody($ct, $raw) {
		$cts = explode(";", $ct);
		// default encoding is ascii
		$encoding = "US-ASCII";

		// extract charset from content-type header
		foreach($cts as $c) {
			$c = trim($c);

			if (strpos($c, "charset=") !== FALSE) {
				$cs = explode("=", $c);
				$encoding = trim($cs[1]);
			}
		}
		// if cts has no second part, encoding is default
		if (count($cts) < 2) {
			$this->body = $raw;
			return;
		}
		switch ($encoding) {
			case 'quoted-printable':
				$this->body = quoted_printable_decode($raw);
				break;
			default:
				$this->body = mb_convert_encoding($raw, "UTF-8", $encoding);
		}
	}
	// for content-type: message/rfc822
	private function parseMessageBody($ct, $raw) {
		$mail = new MailParser($raw, true);
		$out = "---------- Weitergeleitete Nachricht ----------\n";
		$out .= $mail->getBody();
		$this->body = $out;
	}

	private function parseBody($raw) {
		$contentType = trim($this->getHeader("content-type"));
		$transferEncoding = trim($this->getHeader("content-transfer-encoding"));
		// encode to uft-8 if nessesary
		if ($transferEncoding != null && strtolower($transferEncoding) !== '8bit') {
			$raw = mb_convert_encoding($raw, "UTF-8", $transferEncoding);
		}

		$splitted = explode(";", $contentType);

		// content-type is plain
		if (count($splitted) < 1) {
			$this->body = $raw;
			return;
		}

		$splitted[0] = trim($splitted[0]);

		switch ($splitted[0]) {
			case "message/rfc822":
				$this->parseMessageBody($contentType, $raw);
				break;
			case "text/plain":
				$this->parseTextPlainBody($contentType, $raw);
				break;
			case "multipart/signed":
				$this->parseMultipartSignedBody($contentType, $raw);
				break;
			case "multipart/alternative":
			case "multipart/mixed":
			case "multipart/unsigned":
				$this->parseMultipartUnsignedBody($contentType, $raw);
				break;
			default:
				$this->body = $raw;
				break;
		}
	}
	public function includeBasicHeaders() {

		$from = trim($this->getHeader("from"));
		$to = trim($this->getHeader("to"));
		$subject = trim($this->getHeader("subject"));
		$date = trim($this->getHeader("date"));

		$newBody = "";
		if ($from !== "") {
			$newBody .= "From: $from\n";
		}
		if ($to !== "") {
			$newBody .= "To: $to\n";
		}
		if ($date !== "") {
			$newBody .= "Date: $date\n";
		}
		if ($subject !== "") {
			$newBody .= "Subject: $subject\n\n";
		}
		$newBody .= $this->body;
		$this->body = $newBody;
	}
	public function __construct($mail, $showBasicHeaders=false) {

		$this->mail = $mail;
		$this->showBasicHeaders = $showBasicHeaders;
		$this->headers = array();
		$this->body = "";
		$this->attachments = array();
		$this->error = "ok";
		$this->parts = [];
		$this->newLine = "\r\n";

		$index = strpos($mail,"\r\n\r\n");
		$size = 4;

		if ($index === FALSE) {
			$index = strpos($mail, "\n\n");
			$size = 2;
			$this->newLine = "\n";
		}

		if ($index === FALSE) {
			$this->parseHeaders($mail);
		} else {
			$this->parseHeaders(substr($mail, 0, $index), $this->newLine);
			$this->parseBody(substr($mail, $index + $size));
		}

		if ($this->showBasicHeaders) {
			$this->includeBasicHeaders();
		}
	}

	public function getHeader($header) {
		$tag = strtolower($header);
		if (isset($this->headers[$tag])) {
			return $this->headers[$tag];
		}
		return "";
	}

	public function getHeaders() {
		return $this->headers;
	}

	public function getBody() {
		return $this->body;
	}

	// not supported by now
	public function getAttachments() {
		return $this->atttachments;
	}
}

?>
