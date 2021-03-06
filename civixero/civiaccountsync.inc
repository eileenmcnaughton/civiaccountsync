<?php

function civixero_capabilities() {

    return array(
        'civixero_contact_push'          => t('Push contacts details to accounts package (for contacts with pending invoices)'),
        'civixero_contactrecur_push'     => t('Push contacts with recurring to accounts package (for contacts with pending invoices)'),
        'civixero_contact_pull'          => t('Pull contact details from accounts package'),
        'civixero_invoice_push'          => t('Push new invoices from CiviCRM to Xero'),
        'civixero_invoice_pull'          => t('Pull invoice updates from accounts package'),
        'civixero_payment_pull'          => t('Pull payments from Xero'),
        //'civixero_invoice_push_cancel'   => t('Cancel invoices in accounts package based on them being cancelled in CiviCRM'),
        //'civixero_invoice_cancel_inCivi' => t('Cancel invoices in CiviCRM based on them being cancelled voided or deleted in accounts package')
    );

}

function civixero_init() {
  $xero = &drupal_static('civixero');
  civicrm_initialize();
  $xeroSync = new CRM_Xerosync_Base();
  $xero = $xeroSync->getSingleton();
  return $xero;
}

/*
 * Get all contact details for 'active' (generally pending) invoices where the
 * contact has changed since last update. Push to Xero
 */
function civixero_contact_push($last_run) {

    $xero    = &drupal_static('civixero');
    $results = civiaccountsync_getCiviCRMContactstoUpdate($last_run);

    foreach ($results as $contribution) {

        $accountsContactID = db_select('cas_contacts', 'c')
            ->fields('c', array('acc_cid'))
            ->condition('civicrm_cid', $contribution['contact_id'], '=')
            ->execute()
            ->fetchField();

        $civiCRMcontact  = civiaccountsync_getContactDetailsFromCivi($contribution['contact_id']);

        $accountsContact = civixero_contact_mapCiviCRMParamstoAccountsParams($civiCRMcontact, $accountsContactID);
        $result = @$xero->Contacts($accountsContact);

        // Catch oauth errors
        if (is_string($result) and $errors = civixero_validate_response($result)) {
            civiaccountsync_error('Xero Error: <pre>' . print_r($errors, true) . '</pre>');
            return;
        }

    }

    civiaccountsync_message(t('There were @count contacts with pending updates @somehow sent to Xero',
        array(
            '@count'   => count($results) ? count($results) : 'no',
            '@somehow' => count($results) ? 'that were' : 'to be'
        )
    ));

    return true;

}
/*
 * Get all contact details for 'active' (generally pending) invoices where the
* contact has changed since last update. Push to Xero
*
* @param integer $last_run timestamp of last completed sync
* @param integer $throttle limit to do @ once  - not actually passing this in from anywhere yet
*/
function civixero_contactrecur_push($last_run, $throttle = 800) {
  try {
    $result = civicrm_api3('xerosync', 'contactpush', array());
  }
  catch (Exception $e) {
    dpm($e->getMessage());
  }
  civiaccountsync_stashCiviCRMRecurContactstoUpdate($last_run);
  return;
}
/*
 *   get contacts from Xero & store / update them in civixero_contacts
 *   use 'civixero_lastxerocontactget' to control date range
 *   No analysis done in this function - just populate the table for matching
 *
 */
function civixero_contact_pull($last_run) {
  if ($last_run > strtotime('2 days ago')) {
    //we will get a min of 2 days to be sure ...
    $last_run = strtotime('2 days ago');
  }
  $params = array(
    'start_date' => date("Y-m-d H:m:s", $last_run),
  );
  $result = civicrm_api3('xerosync', 'contactpull', $params);
  return true;
}

function civixero_invoice_pull($last_run) {

    // need table prefixes set up (or Drupal + Civi in one db) to run this job
    if (!db_table_exists('civicrm_contribution')) {
        civiaccountsync_error(t('CiviXero: Unable to find table: civicrm_contribution while attempting to push contributions to Xero - have you set your civicrm table prefixes in settings.php?'));
        return false;
    }

    // see comments at bottom of the function regarding 'midnight yesterday' offset
    $from_date = $last_run ? date('c', strtotime('yesterday', $last_run)) : 0;

    $result = @drupal_static('civixero')->Invoices(false, $from_date, array(
        "Type" => "ACCREC"
    ));

    // Check for oauth errors
    if (is_string($result) and $errors = civixero_validate_response($result)) {
        civiaccountsync_error('Xero Error: <pre>' . print_r($errors, true) . '</pre>');
        return;
    }

    civicrm_initialize(true);

    $contribution_statuses = array_flip(CRM_Contribute_PseudoConstant::contributionStatus());

    // Additionally, we would want to check for any invoices that were downloaded previously, but
    // couldn't be added. Say, if the contact hadn't been downloaded yet.

    foreach (db_select('cas_invoices', 'c')
        ->fields('c', array('additional_data'))
        ->condition('civicrm_iid', 0, '=')
        ->execute() as $previously_failed)
            $result['Invoices']['Invoice'][] = unserialize($previously_failed->additional_data);


    if (isset($result['Invoices']['Invoice'])) {

        foreach ($result['Invoices']['Invoice'] as $invoice) {

            $record = new stdClass();
            $update = array();
            //$cas_iid = db_result(db_query("SELECT cas_iid FROM {cas_invoices} WHERE civicrm_iid > 0 AND acc_iid = '%s'", $invoice['InvoiceID']));

            $cas_iid = db_select('cas_invoices', 'c')
                ->fields('c', array('cas_iid'))
                ->condition('acc_iid', $invoice['InvoiceID'], '=')
                //->condition('civicrm_iid', 0, '>')
                ->execute()
                ->fetchField();

            if (!empty($cas_iid)) {
                $update = 'cas_iid';
                $record->cas_iid = $cas_iid;
            }

            if (!isset($invoice['InvoiceNumber']) or empty($invoice['InvoiceNumber'])) {
                civiaccountsync_error("Xero returned an invoice with no invoice number. Debug info:<br /><pre>" . print_r($result, true) . '</pre>');
                continue;
            }

            if (!isset($invoice['Contact']['ContactID']) or empty($invoice['Contact']['ContactID'])) {
                civiaccountsync_error(t("Xero returned an invoice with no external contact identifier. Cannot create contribution in CiviCRM."));
                continue;
            }

            // Map Xero invoice statuses to Civi contribution statuses
            switch ($invoice['Status']) {
                case 'PAID':
                    $status = 'Completed';
                    break;
                case 'DELETED':
                case 'VOIDED':
                case 'DRAFT':
                    $status = 'Cancelled';
                    break;
                case 'AUTHORISED':
                case 'SUBMITTED':
                    $status = 'Pending';
                    break;

            }

            if (isset($status)) {
                $record->civicrm_status = $status;
            }

            $record->acc_status     = $invoice['Status'];
            $record->civicrm_iid    = 0;    // Zero indicates no contribution record has been created yet
            $record->acc_iid        = $invoice['InvoiceID'];
            $record->acc_cid        = $invoice['Contact']['ContactID'];
            $record->acc_last_mod   = strtotime($invoice['UpdatedDateUTC']);
            $record->acc_amount     = $invoice['Total'];
            //$record->civicrm_amount = $invoice['Total'];

            // chuck the whole invoice array into additional_data - if the code below fails, we may need to come back to it another time
            $record->additional_data = serialize($invoice);

            drupal_write_record('cas_invoices', $record, $update);

            // Some additional stuff to add the invoice directly to Civi if we're able to - andyw
            // (I think we should be safe to do that here, rather than having invoices go into an approval queue like contacts do)

            // does the contact exist in the CiviCRM Contacts table?

            if (!$contact_id = civiaccountsync_get_contact_id_from_external_id($record->acc_cid)) {

                if (db_query('SELECT COUNT(*) FROM {cas_contacts} WHERE acc_cid = :acc_cid', array(':acc_cid' => $record->acc_cid))->fetchField())  {
                    // Is a pending contact ..
                    civiaccountsync_warning(t(
                        'Unable to add invoice for @that_contact in CiviCRM, as the contact has been downloaded but not yet added. ' .
                        'Please visit !this_link to add all pending contacts, then re-run Invoice Pull.',
                        array(
                            '@that_contact'  => $invoice['Contact']['Name'],
                            '!this_link'     => isset(drupal_static('civiaccountsync')->is_drush) ? url('admin/cas/contact', array('absolute' => true)) : l('this page', 'admin/cas/contact')
                        )
                    ));
                } else {
                    // Who? ..
                    civiaccountsync_warning(t(
                        'Unable to add invoice for @that_contact in CiviCRM, as the contact has not been downloaded yet. ' .
                        'Please run Contact Pull first and add the contact, then re-run Invoice Pull',
                        array(
                            '@that_contact' => $invoice['Contact']['Name']
                        )
                    ));
                }
                continue;

            }

            // Not sure if this is the greatest test for is_pay_later (is Amount Due > 0?)
            // What happens if the invoice is part-paid? Not sure what we would do in that situation,
            // but hey, this will do for now ..
            if ($is_pay_later = ((float)$invoice['AmountDue'] > 0))
                $status = 'Pending';

            $params = array(
                'version'                => 3,
                'contact_id'             => $contact_id,
                'receive_date'           => $invoice['Date'],
                'is_pay_later'           => $is_pay_later,
                'invoice_id'             => $invoice['InvoiceNumber'],
                'source'                 => 'Xero',
                'contribution_status_id' => $contribution_statuses[$status],
                'financial_type_id'   => variable_get('civixero_financial_type_id', 1),
                'total_amount'           => $invoice['Total']
            );

            // Check to see if this contribution already exists, in which case update the record
            if ($existing_contribution_id = CRM_Core_DAO::singleValueQuery("
                SELECT id FROM civicrm_contribution WHERE invoice_id = %1
            ", array(
                1 => array($invoice['InvoiceNumber'], 'String')
            ))) {
                $params['id'] = $existing_contribution_id;
            }

            // Create / update contribution
            $api_result = civicrm_api('Contribution', 'Create', $params);

            // Inform user of outcome ..
            if ($api_result['is_error']) {

                civiaccountsync_error(t(
                    'Unable to @do contribution for @someone while running CiviXero Invoice Pull because @excuse',
                    array(
                        '@do'      => isset($params['id']) ? 'update' : 'create',
                        '@someone' => $invoice['Contact']['Name'],
                        '@excuse'  => $api_result['error_message']
                    )
                ));

            } else {

                // Update cas_invoices table with the contribution id if successful
                drupal_write_record('cas_invoices',
                    $record = (object)array(
                        'civicrm_iid' => $api_result['id'],
                        'acc_iid'     => $invoice['InvoiceID']
                    ),
                    'acc_iid'
                );

                civiaccountsync_message(t(
                    'Successfully @did contribution for @someone based on invoice @number',
                    array(
                        '@did'     => isset($params['id']) ? 'updated' : 'added',
                        '@someone' => $invoice['Contact']['Name'],
                        '@number'  => $params['invoice_id']
                    )
                ));
            }

        }

    }

    if (!isset($result['Invoices']['Invoice']) or empty($result['Invoices']['Invoice']))
        civiaccountsync_message(t('No pending invoices in Xero'));

    // Because CiviAccountSync is now responsible for keeping track of the job's last run time, it will go ahead and store
    // the last_run time as <current time>. So I'm subtracting the day at the top of the function, before the request to Xero is made.
    // Hopefully this ends up having the same result (?) - andyw

    /*
    //I'm sure this could be more elegant but to avoid server time issues just set last sync date to yesterday since
    //volume isn't such that a day repeats matters & otherwise timezone stuff needs to be right on the ball in code & on the server
    $yesterday = mktime(0, 0, 0, date("m"), date("d") - 1, date("Y"));
    //set now as last synced time
    variable_set('civixero_lastxeroinvoiceget', date_convert($yesterday, DATE_UNIX, DATE_ARRAY));
    */

    return true;

}

function civixero_invoice_push($last_run) {

    civicrm_initialize(true);
    $xero = &drupal_static('civixero');

    // need table prefixes set up (or Drupal + Civi in one db) to run this job
    if (!db_table_exists('civicrm_contribution')) {
        civiaccountsync_error(t('CiviXero: Unable to find table: civicrm_contribution while attempting to push contributions to Xero - have you set your civicrm table prefixes in settings.php?'));
        return false;
    }

    // Have changed this slightly to mark contributions as needing an update in accounts package when updated in Civi - so
    // get a list of ids needing updates first, then query the rest of the info in a similar way to before. Have also updated the code
    // to (hopefully) handle any type of contribution, not just event-based ones.

    $pending_contributions = civiaccountsync_get_pending('contribution', array('create', 'update', 'cancel'));
    $contribution_ids      = implode(',', array_keys($pending_contributions));

    // Query all contributions marked as needing an update in Xero (plus associated participant / membership data where applicable)
    $sql = "

            SELECT contribution.id AS contribution_id, contribution.payment_instrument_id AS payment_instrument,
                   contribution.contribution_status_id, contribution.financial_type_id, contribution.invoice_id, contribution.is_pay_later,
                   contribution.receive_date, contribution.total_amount, contribution.trxn_id, cct.name AS contribution_type,
                   contact.id AS contact_id, contact.display_name, contact.first_name, contact.last_name, contact.external_identifier,
                   participant.id AS participant_id, participant.register_date, participant.registered_by_id, participant.fee_amount as participant_fee_amount,
                   ov.label AS block, pending.action AS sync_action

              FROM civicrm_contribution contribution

        INNER JOIN civicrm_financial_type cct ON cct.id = contribution.financial_type_id
        INNER JOIN civicrm_contact contact ON contact.id = contribution.contact_id
        INNER JOIN cas_pending pending ON pending.entity_id = contribution.id

         LEFT JOIN civicrm_participant_payment pp ON pp.contribution_id = contribution.id
         LEFT JOIN civicrm_participant participant ON pp.participant_id = participant.id
         LEFT JOIN civicrm_event ev ON participant.event_id = ev.id
         LEFT JOIN civicrm_option_value ov ON ov.value = ev.event_type_id
         LEFT JOIN civicrm_option_group og ON ov.option_group_id = og.id
         LEFT JOIN civicrm_membership_payment mp ON mp.contribution_id = contribution.id
         LEFT JOIN civicrm_membership membership ON mp.membership_id = membership.id
         LEFT JOIN cas_invoices invoices ON contribution.id = invoices.civicrm_iid

             WHERE contribution.id IN ($contribution_ids)
               AND pending.entity_type = 'Contribution'
          GROUP BY contribution.id
          ORDER BY contribution.id

    ";

    $count    = 0;
    $failures = 0;
    $results  = array();

    foreach (db_query($sql) as $invoice) {

        $invoice = (array)$invoice;

        /*
        if (!$errors = module_invoke_all('invoice_prerequisites', $invoice, $xero)) {
            civiaccountsync_error("<pre>" . print_r($errors, true) . "</pre>");
            return false;
        }
        */

        //if multiple participants one line item each
        $invoice['line_items'] = civiaccountsync_getLineItems($invoice);
        $invoice['action']     = $pending_contributions[$invoice['contribution_id']];

        //cc denotes 'is credit card' & is pre-pended to credit card description in Xero
        $cc = ($invoice['payment_instrument'] == 1 && $invoice['contribution_status_id'] == 1) ? 'cc' : '';

        $xeroID = "";
        if ($invoice['external_identifier']) {
            $xeroIDValue = substr($invoice['external_identifier'], 0, 8) . "-" . substr($invoice['external_identifier'], 8, 4) . "-" . substr($invoice['external_identifier'], 12, 4) . "-" . substr($invoice['external_identifier'], 16, 4) . "-" . substr($invoice['external_identifier'], 20);
            $xeroID = "<ContactID>" . $xeroIDValue . "</ContactID>";
        }

        //$accountType = _getAccountType($invoice['block']);

        $invoice['acc_iid'] = db_query(
            "SELECT acc_iid FROM cas_invoices WHERE civicrm_iid = :contribution_id",
            array(':contribution_id' => $invoice['contribution_id'])
        )->fetchField();

        $invoice['acc_cid'] = db_query(
            "SELECT acc_cid FROM cas_contacts WHERE civicrm_cid = :contact_id",
            array(':contact_id' => $invoice['contact_id'])
        )->fetchField();

        if (civixero_invoice_create($invoice, $cc, 200)) {
            $count++;
            civiaccountsync_remove_pending('Contribution', $invoice['contribution_id'], $invoice['action']);
        } else {
            $failures++;
        }

    }

    // TODO: run civiaccountsync_update_pending()

    if ($failures)
        civiaccountsync_error(t("@some invoices failed to sync with Xero", array('@some' => $failures)));

    civiaccountsync_message(t("@some invoices were synced with Xero", array('@some' => $count ? $count : 'No')));

    return true;

}

function civixero_invoice_complete_paid($last_run) {

    $xero = &drupal_static('civixero');

    require_once "api/api.php";

    // will fail if unable to join against civicrm_contributions, ie: if table prefixes have not been set
    if (!db_table_exists('civicrm_contribution')) {
        civiaccountsync_error(t('CiviXero: Unable to find table: civicrm_contribution while attempting to complete paid invoices - have you set your civicrm table prefixes in settings.php?'));
        return false;
    }

    //get pending registrations
    $sql = "SELECT civicrm_iid, civicrm_amount, acc_amount, cas_iid, acc_last_mod
              FROM {cas_invoices} cas
         LEFT JOIN {civicrm_contribution}  civi ON cas.civicrm_iid = civi.id
             WHERE civi.contribution_status_id = 2
               AND `acc_status` = 1";


    foreach (db_query($sql) as $data) {

        $params = array(
            'contribution_id'        => $data->civicrm_iid,
            'version'                => '3',
            'return.total_amount'    => 1,
            'return.contact_id'      => 1,
            'return.contribution_id' => 1,
            'return.receive_date'    => 1
        );

        $contribution = civicrm_api('Contribution', 'Get', $params);
        $amount       = $contribution['values'][$params['contribution_id']]['total_amount'];

        //TODO implement this as api call - no participant_payment_get yet
        //$participantID = db_result(db_query("SELECT participant_id from {civicrm_participant_payment} where contribution_id = " . $data->civicrm_iid));
        $participantID = CRM_Core_DAO::singleValueQuery("
            SELECT participant_id FROM civicrm_participant_payment WHERE contribution_id = %1
        ", array(
              1 => array($data->civicrm_iid, 'Integer')
           )
        );

        //TODO but no time soon! can't return_participant_fee_amount because api drops it
        $participant_params = array(
            'version' => '3',
            'participant_id' => $participantID
        );
        $participant_result = civicrm_api('participant', 'Get', $participant_params);

        if ($amount != $participant_result['values'][$participantID]['participant_fee_amount'] && $amount != $data->civicrm_amount) {
            civiaccountsync_updateParticipantFeeAmount($participantID, $amount);
        }


        $baseIPN     = new CRM_Core_Payment_BaseIPN();
        $transaction = new CRM_Core_Transaction();

        $input['component']             = 'event';
        $input['total_amount']          = $amount;
        $input['amount']                = $amount; // goes into the civicrm_financial_trxn table as 'total amount'
        $input['net_amount']            = $amount;
        $input['is_test']               = 0;
        $input['fee_amount']            = 0;
        $input['payment_instrument_id'] = 5; // per - civicrm/admin/options/payment_instrument&group=payment_instrument&reset=1
        $input['receive_date']          = date('Y-m-d', $data->acc_last_mod);
        $ids['contact']                 = $contribution['values'][$params['contribution_id']]['contact_id'];
        $ids['contribution']            = $contribution['id'];
        $ids['participant']             = $participantID;
        $ids['event']                   = $participant_result['values'][$participantID]['event_id'];

        if (! $baseIPN->validateData($input, $ids, $objects, false)) {
            civiaccountsync_error(t('Civixero: BaseIPN failed to validate data while attempting to complete paid invoices.'));
            continue;
        }

        $baseIPN->completeTransaction($input, $ids, $objects, $transaction, false);

        $table  = 'cas_invoices';
        $record = new stdClass();

        $record->civicrm_iid    = $data->civicrm_iid;
        $record->cas_iid        = $data->cas_iid;
        $record->civicrm_status = 1;

        drupal_write_record($table, $record, 'cas_iid');

    }

}

function civixero_invoice_push_cancel($last_run) {

    $xero = &drupal_static('civixero');

    // will fail if unable to join against civicrm_contributions, ie: if table prefixes have not been set
    if (!db_table_exists('civicrm_contribution')) {
        civiaccountsync_error(t('CiviXero: Unable to find table: civicrm_contribution while attempting to cancel invoices in accounts package - have you set your civicrm table prefixes in settings.php?'));
        return false;
    }

    //get pending registrations
    $sql = "SELECT civicrm_iid, civicrm_amount, acc_amount, cas_iid,acc_iid
              FROM {cas_invoices} cas
              LEFT JOIN {civicrm_contribution}  civi ON cas.civicrm_iid = civi.id
              WHERE civi.contribution_status_id =3
              AND `acc_status` =2";

      foreach (db_query($sql) as $data) {

        $cancelledInvoice = array(

          'Invoice' => array(
            'InvoiceID'       => $data->acc_iid,
            'InvoiceNumber'   => $data->civicrm_iid,
            'Type'            => 'ACCREC',
            'Reference'       => 'Cancelled',
            'Date'            => date('Y-m-d', strtotime('now')),
            'DueDate'         => date('Y-m-d', strtotime('now')),
            'Status'          => 'DRAFT',
            'LineAmountTypes' => 'Exclusive',
            'LineItems' => array(
              'LineItem' => array(
                'Description' => 'Cancelled',
                'Quantity'    => 0,
                'UnitAmount'  => 0,
                'AccountCode' => 200
              )
            )
          )
        );

        $invoice_result = $xero->Invoices($cancelledInvoice);
      }


}

function civixero_invoice_cancel_inCivi($last_run) {

    if (!db_table_exists('civicrm_contribution')) {
        civiaccountsync_error(t('CiviXero: Unable to find table: civicrm_contribution while attempting to complete paid invoices - have you set your civicrm table prefixes in settings.php?'));
        return false;
    }

    //get pending registrations
    $sql = "
        SELECT civicrm_iid, civicrm_amount, acc_amount, cas_iid
        FROM {cas_invoices} cas
        LEFT JOIN {civicrm_contribution}  civi ON cas.civicrm_iid = civi.id
        WHERE civi.contribution_status_id = 2
        AND `acc_status` = 3
    ";

    foreach (db_query($sql) as $data) {

        $getresult = civicrm_api('Contribution', 'Get', array(
            'contribution_id' => $data->civicrm_iid,
            'version' => 3,
            'sequential' => 1
        ));

        $params = $getresult['values'][0];
        $params['version'] = 3;
        $params['contribution_status_id'] = 3;
        $params['id'] = $getresult['id'];

        $addresult = civicrm_api('Contribution', 'Create', $params);

    }
}

function civixero_payment_pull($last_run) {
  $xero = @drupal_static('civixero');
  $result = $xero->BankTransactions(false, date("Y-m-d H:m:s", $last_run));

  if (is_string($result) and $errors = civixero_validate_response($result)) {
    civiaccountsync_error('Xero Error: <pre>' . print_r($errors, true) . '</pre>');
    return;
  }

  if (isset($result['BankTransactions']['BankTransaction'])) {
    foreach ($result['BankTransactions']['BankTransaction'] as $transaction) {
      $record = new stdClass();
      $update = array();
      $cas_pid = db_select('cas_payments', 'c')
      ->fields('c', array('cas_pid'))
      ->condition('x_btid', $transaction['BankTransactionID'], '=')
      ->execute()
      ->fetchField();

      if (!empty($cas_pid)) {
        $update = 'cas_pid';
        $record->cas_pid = $cas_pid;
      }

   //   $record->reconciled = $transaction['IsReconciled'];
      $record->x_btid = 'ghj';$transaction['BankTransactionID'];
      $record->last_sync = strtotime('now');
      $record->t_type = $transaction['Type'];
   //   $record->additional_data = serialize($transaction);

     drupal_write_record('cas_payments', $record, $update);
    }
  }
}

