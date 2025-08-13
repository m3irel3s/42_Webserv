<?php
echo "Content-Type: text/html\r\n\r\n";
?>
<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<title>Webserv CGI Demo</title>
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<!-- Link to your main stylesheet -->
	<link rel="stylesheet" href="/style.css">
	<style>
		/* Extra tweaks for CGI output */
		.cgi-section { margin-bottom: 2.5rem; }
		.cgi-badge {
			display: inline-block;
			background: var(--primary, #6C63FF);
			color: #fff;
			border-radius: 12px;
			padding: 2px 12px;
			font-size: 1em;
			margin-left: 8px;
		}
		.cgi-post-body {
			background: var(--card-bg, #222c);
			border-radius: 8px;
			padding: 12px;
			font-family: 'Fira Mono', 'Consolas', monospace;
			color: var(--light, #F0F5FF);
			margin-top: 0.5em;
			white-space: pre-wrap;
		}
		.cgi-env-table {
			width: 100%;
			border-collapse: collapse;
			background: rgba(255,255,255,0.02);
			font-size: 1em;
		}
		.cgi-env-table th, .cgi-env-table td {
			padding: 6px 10px;
			border-bottom: 1px solid rgba(255,255,255,0.08);
		}
		.cgi-env-table th {
			background: rgba(255,255,255,0.04);
			text-align: left;
			color: var(--light, #F0F5FF);
		}
		.cgi-env-table tr:last-child td {
			border-bottom: none;
		}
	</style>
</head>
<body>
<div class="container">
	<h1>Webserv CGI Demo</h1>
	<div class="cgi-section">
		<h2>Request Method <span class="cgi-badge"><?php echo htmlspecialchars($_SERVER['REQUEST_METHOD']); ?></span></h2>
		<?php if ($_SERVER['REQUEST_METHOD'] === 'POST'): ?>
			<h3>POST Body</h3>
			<div class="cgi-post-body">
				<?php echo htmlspecialchars(file_get_contents('php://input')); ?>
			</div>
		<?php endif; ?>
	</div>
	<div class="cgi-section">
		<h2>Environment Variables</h2>
		<table class="cgi-env-table">
			<thead>
				<tr><th>Variable</th><th>Value</th></tr>
			</thead>
			<tbody>
<?php
reset($_SERVER);
while (list($key, $value) = each($_SERVER)) {
	printf('<tr><td>%s</td><td>%s</td></tr>', htmlspecialchars($key), htmlspecialchars($value));
}
?>
			</tbody>
		</table>
	</div>
	<?php if ($_SERVER['REQUEST_METHOD'] === 'POST' && count($_POST)): ?>
	<h3>Parsed POST Fields</h3>
	<ul>
	<?php
	$keys = array_keys($_POST);
	for ($i = 0; $i < count($keys); $i++) {
		$k = $keys[$i];
		$v = $_POST[$k];
		echo '<li><b>' . htmlspecialchars($k) . '</b>: ' . htmlspecialchars($v) . '</li>';
	}
	?>
	</ul>
<?php endif; ?>
</div>
</body>
</html>
