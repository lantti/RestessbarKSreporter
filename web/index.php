<?php
include "libchart/classes/libchart.php";

$hmac_key = '1234567890';
$db_host = 'localhost';
$db_user = 'db_user';
$db_pass = 'ultrasecret';

function parse_input_form()
{
	global $hmac_key;

	if (!isset($_POST['mes'], $_POST['time'], $_POST['sig']))
	{
		return array();
	}

	$mes_str = "mes=" . $_POST['mes'] . "&time=" . $_POST['time'] . 
		"&sig=";
	$hmac = hash_hmac('sha1', $mes_str, hex2bin($hmac_key), true);
	$hmac = base64_encode($hmac);
	$hmac = str_replace('+','-',$hmac);
	$hmac = str_replace('/','_',$hmac);
	$hmac = str_replace('=','',$hmac);
	if ($hmac !== $_POST['sig'])
	{
		return array();
	}	

	$datapoints = explode('_', $_POST['mes']);
	$datapoints = array_filter($datapoints);
	foreach ($datapoints as &$value)
	{
		if (is_numeric($value))
		{
			$value = (int) $value;
		}
		else
		{
			return array();
		}
	}
	unset($value);

	if (is_numeric($_POST['time']))
	{
		$datapoints['time'] = (int) $_POST['time'];
	}
	else
	{
		return array();
	}
	return $datapoints;
}

function save_measurements(array $datapoints)
{
	global $db_host;
	global $db_user;
	global $db_pass;

	$val = 0;
	$sub = 0;
	$time = $datapoints['time'];
	unset($datapoints['time']);

	$mysqli = new mysqli($db_host, $db_user, $db_pass, 'rebs_graph');
	if ($mysqli->connect_errno) 
	{
		return false;
	}

	if (!($stmt = $mysqli->prepare("INSERT INTO measurements(time, sub, val) VALUES (?, ?, ?)")) || !$stmt->bind_param('iii', $time, $sub, $val))
	{
		return false;
	}

	foreach ($datapoints as $sub => $val)
	{
		$stmt->execute();
	}
	$stmt->close();
	return true;
}


if ($_SERVER['REQUEST_METHOD'] == 'POST')
{
	$datapoints = parse_input_form();
	if ($datapoints && save_measurements($datapoints))
	{
		header('Content-Type: text/plain');
		echo 'ok';
	}
	else
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
	}
}
elseif ($_SERVER['REQUEST_METHOD'] == 'GET')
{
	global $db_host;
	global $db_user;
	global $db_pass;

	$timestamp = time();

	$mysqli = new mysqli($db_host, $db_user, $db_pass, 'rebs_graph');
	if ($mysqli->connect_errno) 
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
		return;
	}

	if (isset($_GET['b']) && ctype_digit($_GET['b']))
	{
		$begin = (int)$_GET['b'];
	}
	else
	{
		if (($res = $mysqli->query("SELECT MAX(time) FROM measurements")) && ($row = $res->fetch_row()))
		{
			$begin = (int)$row[0];
		}
		else
		{
			header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
			return;
		}

	}

	if (isset($_GET['e']) && ctype_digit($_GET['e']))
	{
		$end = (int)$_GET['e'];
	}
	else
	{
		$end = max($begin - 86400 , 0);
	}

	if ($begin < $end)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
		return;
	}

	if (!($stmt = $mysqli->prepare("SELECT * FROM measurements WHERE time BETWEEN ? AND ?")) || !$stmt->bind_param('ii', $end, $begin))
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
		return;
	}
	$stmt->execute();
	$stmt->store_result();

	$val = NULL;
	$sub = NULL;
	$time = NULL;
	$dataset = new XYDataSet();
	if (!$stmt->bind_result($time, $sub, $val)) 
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found"); 
		return;
	}

	while ($stmt->fetch())
	{
		if ($sub == 0)
		{
			$dataset->addPoint(new Point($time, $val));
		}
		else
		{
			$dataset->addPoint(new Point('', $val));
		}
	}
	$width = $stmt->num_rows * 10;
	$stmt->free_result();
	$stmt->close();
	$chart = new LineChart($width, 500);
	$chart->setDataSet($dataset);
	header("Content-type: image/png");
	$chart->render();
}
?>
