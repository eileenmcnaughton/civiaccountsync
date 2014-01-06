<?php
// $Id$


/*
 *   get invoices from Accounts & store / update them in civicrm_account_invoice
 *   use 'civixero_lastxeroinvoiceget' to control date range
 *
 */
function civiaccountsync_invoice_pull($accountsObject) {
  $last_run = variable_get('civixero_lastxeroinvoiceget', 0);
  if ($last_run > strtotime('2 days ago')) {
    //we will get a min of 2 days to be sure ...
    $last_run = strtotime('2 days ago');
  }
  $params = array(
    'start_date' => date("Y-m-d H:m:s", $last_run),
  );
  $result = civicrm_api3('xerosync', 'invoicepull', $params);
  //set now as last synced time
  variable_set ( 'civixero_lastxeroinvoiceget', strtotime('now'));
}

/*
 * push invoices from CiviCRM to accounting package. Function generates an array of
 * values for pending invoices & calls the accounts specific function
 *
 * @$accObject
 */
function civiaccountsync_invoice_push($accObject) {
  //todo  - how best to restructure this? I need an sql contribution vs civicrm_accountinvoice but
  //what other info do I get in the same sql & what do I get using API?
  $sql = "
SELECT civicrm_contribution.id AS id
FROM civicrm_contribution civicrm_contribution
LEFT JOIN civicrm_account_invoice ci ON civicrm_contribution.id = ci.contribution_id

WHERE civicrm_contribution.receive_date >= '2014-01-01'
AND civicrm_contribution.id is not null
AND civicrm_contribution.contribution_status_id <> 3
AND ci.contribution_id IS NULL
ORDER BY civicrm_contribution.id
";

   // module_invoke_all('invoice_prerequisites',$invoice,$accObject);
  $count = 0;
  $dao = CRM_Core_DAO::executeQuery($sql);

  while ($dao->fetch()) {
   $invoice = (array) $dao;
    _civiaccountsync_invoice_create($accObject, $invoice);
    return;
    $count++;
  }
    if(!empty($count)){
      drupal_set_message(t("$count invoices were created"));
    }
    variable_set ( 'civixero_lastxeroinvoiceput', strtotime('now'));
}

function _civiaccountsync_invoice_create($accountsObject, $value) {

  $xero = new CRM_Xerosync_Invoice();
  $contributionID = $value['id'];
  $invoice = civicrm_api3('account_invoice', 'getderived', array('id' => $contributionID));
  dpm($invoice);
  $new_invoice = $xero->mapToAccounts($invoice['values'][$contributionID], NULL);
dpm($new_invoice);
  $invoice_result = $accountsObject->Invoices ($new_invoice);
  dpm($invoice_result);
  $errors = civicaccountsync_civixero_validate_response($invoice_result ) ;
  if (is_array($errors)){
    drupal_set_message("<pre>".print_r($errors,true)." for invoice $value[contribution_id] for contact $value[contact_id] : $value[display_name]</pre>");
    return $errors;
  }

    $record = new stdClass ();
    $record->acc_iid = $invoice_result ['Id'];
    $record->civicrm_iid = $value ['contribution_id'];
    $record->status = $value ['contribution_status_id'];
    $record->civicrm_amount = $value ['total_amount'];
    $record->acc_status = 2;
    $record->acc_last_mod = strtotime ( 'now' );
    $record->acc_amount = $value ['total_amount'];
    drupal_write_record ( 'civicrm_account_invoice', $record );
    drupal_set_message("created invoice ". $value ['contribution_id'] . " for " . $value ['display_name']);



}

/*
 * Complete any pending contributions that have been paid through the Accounts package.
 * Completing includes sending out e-mails and changing status on contribution and related entities
 * @param object $accountsObject Accounts package object
 */

function civiaccountsync_invoice_complete_paid($accountsObject) {
  require_once "api/api.php";

  //get pending registrations
  $sql = "
    SELECT contribution_id, accounts_invoice_id, accounts_modified_date, cas.id
    FROM civicrm_account_invoice cas
    LEFT JOIN civicrm_contribution  civi ON cas.contribution_id = civi.id
    WHERE civi.contribution_status_id =2
    AND accounts_status_id =1
   ";
  $dao = CRM_Core_DAO::executeQuery($sql);


  while ($dao->fetch()) {
    $params ['contribution_id'] = $dao->contribution_id;
    $params ['version'] = '3';
    $params ['return.total_amount'] = 1;
    $params ['return.contact_id'] = 1;
    $params ['return.contribution_id'] = 1;
    $params ['return.receive_date'] = 1;
    $contribution = civicrm_api ( 'Contribution', 'Get', $params );
    $amount = $contribution['values'][$params ['contribution_id']]['total_amount'];

    $participantArr = civicrm_api3('participant_payment', 'getsingle', array(
      'contribution_id' => $dao->contribution_id,
      'api.participant.getsingle' => 1,
    ));
    $participant = $participantArr['api.participant.getsingle'];
    // we were altering the amount if a different amount was paid but not so sure this is ok now
  //  if ($amount != $participant_result['values'][$participantID]['participant_fee_amount'] && $amount != $data->civicrm_amount) {
  //    _updateParticipantFeeAmount ( $participantID, $amount );
  //  }

    $baseIPN = new CRM_Core_Payment_BaseIPN ();
    $transaction = new CRM_Core_Transaction ();

    $input ['component'] = 'event';
    $input ['total_amount'] = $amount;
    $input ['amount'] = $amount; // goes into the civicrm_financial_trxn table as 'total amount'
    $input ['net_amount'] = $amount;
    $input ['is_test'] = 0;
    $input ['fee_amount'] = 0;
    $input ['payment_instrument_id'] = 5; // per - civicrm/admin/options/payment_instrument&group=payment_instrument&reset=1
    $input ['receive_date'] = $dao->accounts_modified_date;
    $ids ['contact'] = $contribution ['values'] [$params ['contribution_id']] ['contact_id'];
    $ids ['contribution'] = $contribution ['id'];
    $ids ['participant'] = $participant['id'];
    $ids ['event'] = $participant['event_id'];
    $objects = array();
    if (! $baseIPN->validateData ( $input, $ids, $objects, false )) {
      CRM_Core_Error::fatal('could not validate');
    }
    $baseIPN->completeTransaction ( $input, $ids, $objects, $transaction, false );
  }

}



/*
 * Cancel pending invoices in CiviCRM that have been cancelled in the accounts package
 * @param object $accountsObject accounting object that can interact with accounts package
 */
function civiaccountsync_invoice_push_cancel($accountsObject) {

  //get pending registrations
  $sql = "SELECT contribution_id, cas.id, cas.accounts_invoice_id
    FROM civicrm_account_invoice cas
    LEFT JOIN civicrm_contribution  civi ON cas.contribution_id = civi.id
    WHERE civi.contribution_status_id =3
    AND accounts_status_id = 2";

  $cxresult = db_query ( $sql );
  while ( $data = db_fetch_object ( $cxresult ) ) {

    $cancelledInvoice = array(

        'Invoice' => array(
          'InvoiceID'     => $data->accounts_invoice_id,
          'InvoiceNumber' => $data->contribution_id,
          'Type'          => 'ACCREC',
          'Reference' =>  'Cancelled',
          'Date'  => date('Y-m-d',strtotime(now)),
          'DueDate'  => date('Y-m-d',strtotime(now)),
          'Status'  => 'DRAFT',
          'LineAmountTypes' =>'Exclusive',
          'LineItems' => array(
            'LineItem' => array(
             'Description' => 'Cancelled',
             'Quantity' => 0,
             'UnitAmount'=> 0,
             'AccountCode'=> 200,
             )
           ),
         )
      );

  $invoice_result = $accountsObject->Invoices($cancelledInvoice);
  }
}

/*
 * Cancel pending invoices in CiviCRM based on them being cancelled in the accounts package
 */

function civicaccountsync_invoice_cancel_inCivi($xero){
  //get pending registrations
  $sql = "SELECT  cas.contribution_id
    FROM civicrm_account_invoice cas
    LEFT JOIN civicrm_contribution  civi ON cas.contribution_id = civi.id
    WHERE civi.contribution_status_id =2
    AND accounts_status_id =3
  ";
  $dao = CRM_Core_DAO::executeQuery($sql);

  while ($dao->fetch()) {
    $params['contribution_status_id'] = 3;
    $params['id'] = $dao->contribution_id;
    civicrm_api3('Contribution','Create',$params) ;
  }
}


/*
   * Update the participant fee amount to reflect the amount actually paid. This
   * allows the administrator to change the invoice in the accounts package to
   * reflect an agreement & have the change flow back to the participant record
   * As of writing it is not possible to change a participant fee amount through CiviCRM without
   * using SQL
   *
   * Not implemented for multiple participants yet
   */
function _updateParticipantFeeAmount($participantID, $amount) {
  //find out whether there is one or more line items. A count greater than 0 indicates more than one
  $sql = "SELECT count(id)
          FROM {civicrm_participant}
          WHERE registered_by_id = %d";
  $count = db_result ( db_query ( $sql, $participantID ) );
  if (empty ( $count )) {
    $sql = "UPDATE
         {civicrm_participant}
         SET fee_amount = %d
         WHERE id = %d";
        $result = db_query ( $sql, $amount,$participantID  );
  } else {

    $sql = "SELECT id
          FROM {civicrm_participant}
          WHERE registered_by_id = %d";
    $result = db_query ( $sql, $participantID );
    while ( $reldata = db_fetch_object ( $result ) ) {
      drupal_set_message ( "The amount paid for this invoice has changed. Changing invoices with more than one line item not currently supported in CiviAccountSync<pre></pre>" );

    }

  }
}

/*
 * Wellington Circus Trust function with a future elsewhere
 */
