<?php
	// Read config settings
	if(file_exists('config.php')) {
		require_once('config.php');
	}
	else {
		die('ERROR: Unable to open required file (config.php)');
	}
	
	$connection=mysql_connect($dbServer, $dbUser, $dbPass) or die ("Verbindungsversuch fehlgeschlagen");
	
	mysql_select_db($dbName, $connection) or die("Konnte die Datenbank nicht waehlen.");
	
	$sth = mysql_query("SELECT tp_Timestamp, tp_TargetTemp, tp_MeasuredTemp, tp_HeatingStatus from fm_TempProtocol where DATE_SUB(CURDATE(),INTERVAL 2 DAY) <= tp_Timestamp;");
	
	$rawRows = array();
	$cols = array();
	
	// set up domain column
	$cols[] = array(
		'type' => 'datetime',
		'id' => 'tp_Timestamp',
		'label' => 'Time'
	);
	$cols[] = array(
		'type' => 'number',
		'id' => 'tp_TargetTemp',
		'label' => 'Target Temperature'
	);
	$cols[] = array(
		'type' => 'number',
		'id' => 'tp_MeasuredTemp',
		'label' => 'Measured Temperature'
	);
	$cols[] = array(
		'type' => 'string',
		'id' => 'tp_HeatingStatus',
		'label' => 'Heating status change'
	);
	
	$outRows = array();
	$lastHeatingStatus = 0;
	
	while($r = mysql_fetch_assoc($sth)) {
    	#$rawRows[] = $r;
    	
    	$temp = array();
    	$dt = new DateTime($r['tp_Timestamp']);
    	// decrement month by one cause JavaScript starts month with 0
    	$month = (int)$dt->format('n');
    	$month = $month - 1;
    	$datestring = "Date(" . $dt->format('Y') . ',' . $month . ',' . $dt->format('j,G,i,s') . ")";
		$temp[] = array('v' => $datestring);
		$temp[] = array('v' => $r['tp_TargetTemp']);
		$temp[] = array('v' => $r['tp_MeasuredTemp']);
		
		if ($r['tp_HeatingStatus'] != $lastHeatingStatus) {
			switch ($r['tp_HeatingStatus']) {
				case 0:
					$temp[] = array('v' => 'Heating off');
					break;
				case 1:	
					$temp[] = array('v' => 'Heating on');
					break;
			}
			$lastHeatingStatus = $r['tp_HeatingStatus'];
		} else {
			$lastHeatingStatus = $r['tp_HeatingStatus'];
			$temp[] = array('v' => null);
		}	
				
		$outRows[] = array('c' => $temp);
	}

	
	
	#for ($i = 1; $i < count($rawRows); $i++) {
	#	
	#	$temp = array();
	#	$temp[] = array('v' => $rawRows[$i]['tp_Timestamp']);
	#	$temp[] = array('v' => $rawRows[$i]['tp_TargetTemp']);
	#	$temp[] = array('v' => $rawRows[$i]['tp_MeasuredTemp']);
	#
	#	$outRows[] = array('c' => $temp);
	#}

	$dataTable = json_encode(array(
		'cols' => $cols,
		'rows' => $outRows
	), JSON_NUMERIC_CHECK);
	
	echo $dataTable;
?>