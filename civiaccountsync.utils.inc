<?php

/**
 * Have tried to place helper / utility functions which are generic in this file,
 * and anything more specific to Xero in civixero.utils.inc - but some further rearranging
 * may still be necessary
 * andyw@circle, 22/03/2013
 */

/*
 * Get contact details for any contacts with 'current' (pending) contributions that have
 * changed since the last update
 * (note the contact object rather is the one whose modification date is tracked
 *
 * return array $result array of contributions in pending status where contact modified since last update
 */

// Primarily intended to be used from Drush. Will add all pending contacts, setting params accordingly
function civiaccountsync_add_all_contacts($params = array(), $plugin = null) {

    // Add all pending contacts to CiviCRM
    for ($counter = 0; $cas_cid = civiaccountsync_get_nextUnmatchedContact(); $counter++)
        civiaccountsync_add_contact_to_civicrm($cas_cid, $params);
    if (!$counter)
        civiaccountsync_message(t('No pending contacts to add'));

}

// Add contact identified by cas_cid to CiviCRM, using the specified params
function civiaccountsync_add_contact_to_civicrm($cas_cid, $params) {

    // load contact data for the unmatched contact

    if (!$data = db_select('cas_contacts', 'c')
        ->fields('c')
        ->condition('cas_cid', $cas_cid, '=')
        ->execute()
        ->fetchObject()) {

        civiaccountsync_error(t('Unable to lookup contact data'), 'error');
        return;
    }

    // Not quite sure why we're calling Contact Add form's submit handler here, would probably make
    // more sense to keep the logic in here and get the handler to call us instead, but let's put
    // it on the todo list for now ..

    module_load_include('inc', 'civiaccountsync', 'civiaccountsync.forms');
    civiaccountsync_form_contact_submit($form = array(), $form_state = array(
        'values' => array(
            'cas_cid'             => $cas_cid,
            'first_name'          => $data->acc_first_name,
            'last_name'           => $data->acc_last_name,
            'display_name'        => $data->acc_full_name,
            'email'               => $data->acc_primary_email,
            'additional_data'     => $data->additional_data,
            'do_not_email'        => $params['do_not_email'],
            'is_opt_out'          => $params['is_opt_out'],
            'contact_type'        => $params['contact_type'],
            'plugin'              => $data->plugin,
            'sync_action'         => 'create',
            'external_identifier' => $data->acc_cid
        )
    ));

}

function civiaccountsync_count_pending_contacts() {
  civicrm_initialize();
  return CRM_Core_DAO::singleValueQuery('SELECT COUNT(*) FROM cas_contacts');
}

// Maybe this should be called getCiviCRMContributionsToUpdate?
function civiaccountsync_getCiviCRMContactstoUpdate($last_updated) {

    civicrm_initialize(true);

    //TODO - currently code assumes that 1000 contributions is enough to bring it up-to-date & set last sync to current date
    $result = civicrm_api('Contribution', 'Get', array(
        'version'                => 3,
        'contribution_status_id' => 2,
        'modified_date_low'      => date("Y-m-d", $last_updated),
        'rowCount'               => 1000,
        //'sort'                 => 'civicrm_log.modified_date ASC'
    ));
    if (!$result['is_error']) {
        if (isset($result['values']))
            return $result['values'];
        return array();
    }

    civiaccountsync_error(t(
        'Unable to retrieve pending contributions via API because @excuse',
        array(
            '@excuse' => $result['error_message']
        )

    ));
    return array();
}

/**
 * So, lets add them to the cas_contacts & then retrieve any contacts that need updating in a separate
 * fn. The second fn will want to retrieve the id which this doesn't care about
 * @param integer (timestamp) $last_updated
 * @return void|multitype:
 */
function civiaccountsync_stashCiviCRMRecurContactstoUpdate($last_updated) {

  if(!civicrm_initialize(true)) {
    return;
  }
  $last_updated = date('Y-m-d h-i-s', $last_updated);

  //@todo- would be better to use a civi table here - then we can join without worrying about prefixes
  // but lets just be slack for now. db_query is possibly more robust WRT prefixes
  $sql =
  " INSERT INTO cas_contacts (civicrm_cid)
    SELECT DISTINCT contact_id FROM civicrm_contribution_recur cr
    LEFT JOIN cas_contacts cas ON cas.civicrm_cid = cr.contact_id
    WHERE cas.cas_cid IS NULL AND cr.create_date > '" . $last_updated . "'
   ";
  CRM_Core_DAO::executeQuery($sql);
  return TRUE;
}
function civiaccountsync_get_contact_id_from_external_id($external_id) {
    if ($external_id)
        return CRM_Core_DAO::singleValueQuery("
           SELECT id FROM civicrm_contact WHERE external_identifier = %1
        ", array(
              1 => array($external_id, 'String')
        ));
}

/*
 * Get contact Details out of CiviCRM
 */
function civiaccountsync_getContactDetailsFromCivi($contactID) {

    $contact = civicrm_api('Contact', 'Get', array(
        'id'      => $contactID,
        'version' => 3
    ));
    if (!$contact['is_error'])
        return reset($contact['values']);
    return array();

}

/*
   * Get Line items for invoice. At this stage only secondary participants are being fetched here
   * @param array $invoice Invoice array being prepared for Xero
   * @return array $lineitems if there are some else null
   */
function civiaccountsync_getLineItems(&$invoice) {

    $line_items = array();

    // If an event-based contribution, create line item for each participant where multiple participants exist
    if ($invoice['participant_id'] and $invoice['total_amount'] > $invoice['participant_fee_amount']) {

        require_once 'api/api.php';
        // api let us down use direct sql
        // TODO get api to accept the below
        // $otherParticipants = civicrm_api('Participant','Get',array('version' =>3,'participant_registered_by_id' =>$invoice['participant_id']));

        $dao = CRM_Core_DAO::executeQuery("
           SELECT * FROM civicrm_participant WHERE registered_by_id = %1
        ", array(
              1 => array($invoice['participant_id'], 'Integer')
           )
        );

        while ($dao->fetch()) {
            $apiResult = civicrm_api('Participant', 'Get', array(
                'version' => 3,
                'id'      => $dao->id
            ));
            if (!$apiResult['is_error'] and count($apiResult['values']))
                $line_items[] = reset($apiResult['values']);
        }
    } else {

        // Otherwise, do a standard LineItem Get for the contribution
        /*
        // .. except that doesn't exist in our version of Civi
        $result = civicrm_api('line_item', 'get', $params = array(
            'version'      => 3,
            'entity_table' => 'civicrm_contribution',
            'entity_id'    => $invoice['contribution_id']
        ));

        if (!$result['is_error'] and count($result['values']))
            $line_items = $result['values'];
        */

        // Ok, try this ..
        $line_items = array();
        $dao = CRM_Core_DAO::executeQuery(
            "SELECT * FROM civicrm_line_item WHERE entity_table = 'civicrm_contribution' AND entity_id = %1",
            array(
                1 => array($invoice['contribution_id'], 'Integer')
            )
        );
        while ($dao->fetch()) {
            $line_item = array();
            foreach (
                array(
                    'id',
                    'price_field_id',
                    'label',
                    'qty',
                    'unit_price',
                    'line_total',
                    'participant_count',
                    'price_field_value_id'
                ) as $field_name) {
                    $line_item[$field_name] = $dao->$field_name;
            }
            $line_items[] = $line_item;
        }

    }

    if ($line_items)
        return $line_items;
    return null;

}

/*
 * Get next accounts contact not matched to a CiviCRM record
 */
function civiaccountsync_get_nextUnmatchedContact($plugin = null) {

    $query = db_select('cas_contacts', 'c')
        ->fields('c', array('cas_cid'))
        ->condition('civicrm_cid', 0, '=')
        ->condition('civi_ignore', 0, '=')
        ->isNull('civi_suggested_contact_id');

    if ($plugin)
        $query = $query->condition('plugin', $plugin, '=');

    return $query->execute()->fetchField();

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
function civiaccountsync_updateParticipantFeeAmount($participantID, $amount) {

    //find out whether there is one or more line items. A count greater than 0 indicates more than one
    $count = CRM_Core_DAO::singleValueQuery("
        SELECT COUNT(*) FROM civicrm_participant WHERE registered_by_id = %1
    ", array(
          1 => array($participantID, 'Integer')
       )
    );

    if (!$count) {

        CRM_Core_DAO::executeQuery("
           UPDATE civicrm_participant SET fee_amount = %1 WHERE id = %2
        ", array(
              1 => array($amount, 'Money'),
              2 => array($participantID, 'Integer')
           )
        );

    } else {

        CRM_Core_DAO::executeQuery("
            SELECT id FROM civicrm_participant WHERE registered_by_id = %1
        ", array(
              1 => array($participantID, 'Integer')
           )
        );

        while ($dao->fetch())
            civiaccountsync_message(
                "The amount paid for this invoice has changed. " .
                "Changing invoices with more than one line item not currently supported in CiviAccountSync<pre>" .
                print_r($invoice, true) .
                "</pre>"
            );

    }

}

function civiaccountsync_writepush_result($contact, $result) {
  $record          = new stdClass();

  $record->civicrm_cid = $contact['id'];
  $record->acc_cid = $result['ContactID'];
  $record->last_cas_mod = strtotime($result['UpdatedDateUTC']);
  $record->last_sync = strtotime('now');
  $record->plugin       = 'civixero';
  $cas_cid = db_select('cas_contacts', 'c')
  ->fields('c', array('cas_cid'))
  ->condition('civicrm_cid', $contact['id'], '=')
  ->execute()
  ->fetchField();

  if (!empty($cas_cid)) {
    $update[] = 'cas_cid';
    $record->cas_cid = $cas_cid;
  }

  drupal_write_record('cas_contacts', $record, $update);
}
