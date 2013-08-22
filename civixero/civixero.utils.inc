<?php

/*
 * Utility / helper functions for civixero module
 */

/*
 * Map civicrm Array to Accounts package field names
 * @param array $contact Contact Array as returned from API
 * @param string  accountsID ID from Accounting system
 * @return $accountsContact Contact Object/ array as expected by accounts package
 */
function civixero_contact_mapCiviCRMParamstoAccountsParams($contact, $accountsID) {

  $new_contact = array(
    array(
      "Name" => $contact['display_name'],
      "FirstName" => $contact['first_name'],
      "LastName" =>  $contact['last_name'],
      "EmailAddress" => CRM_Utils_Rule::email($contact['email']) ? $contact['email'] : '',
      "ContactNumber" => $contact['contact_id'],
      "Addresses" => array(
        "Address" => array(
          array(
            "AddressType"  => 'POBOX',  //described in documentation as the default mailing address for invoices http://blog.xero.com/developer/api/types/#Addresses
            "AddressLine1" => $contact['street_address'],
            "City"         => $contact['city'],
            "PostalCode"   => $contact['postal_code']
          )
        )

      ),
      "Phones" => array(
        "Phone" => array(
          "PhoneType" => 'DEFAULT',
          "PhoneNumber" => $contact['phone']
        )
      )
    )
  );
  if (! empty($accountsID)) {
    $new_contact['contactID'] = $accountsID;
  }
  //I thought I should be able to pass by reference but it won't accept it
  $changedContact = module_invoke_all('civixero_contact_alter', $new_contact, $contact);
  if(!empty($changedContact)) {
    $new_contact = $changedContact;
  }
  return $new_contact;

}

function civixero_invoice_create($value, $cc = null, $accountType = null) {

  /*$value['contact_id']
  $value['display_name']
  $value['first_name']
  $value['last_name']
  $value[register_date]
  $value[contribution_id]
  $value['event_code']
  $value['civicrm_participant_fee_amount']
  $value[block]
  $value[event_code]

  //$cc = is credit card ? cc?
  */
  //print 'value = ' . print_r($value, true);

  if (isset($value['participant_id'])) {

      $new_invoice = array(
        array(
          "Type" => "ACCREC",
          "Contact" => array(
            "ContactNumber"             => $value['contact_id'],
            'Name'                      => $value['display_name'],
            'AccountsRecievableTaxType' => 'OUTPUT',
            'FirstName'                 => $value['first_name'],
            'LastName'                  => $value['last_name']
          ),
          "Date"            => substr($value['register_date'], 0, 10),
          "DueDate"         => substr($value['register_date'], 0, 10),
          "Status"          => "SUBMITTED",
          "InvoiceNumber"   => $value['contribution_id'],
          "Reference"       => $cc . $value['event_code'] . " " . $value['display_name'],
          "CurrencyCode"    => /*'NZD'*/ CRM_Core_Config::singleton()->defaultCurrency,
          "LineAmountTypes" => "Inclusive",
          "LineItems" => array(
            "LineItem" => array(
              array(
                "Description" => $value['event_code'] . " " . $value['display_name'],
                "Quantity"    => "1",
                "UnitAmount"  => $value['participant_fee_amount'],
                "AccountCode" => $accountType,
                "Tracking"    => array(
                  "TrackingCategory" => array(
                    /*
                    array(
                      'TaxType' => 'OUTPUT',
                      'Name'    => 'Block / Project',
                      'Option'  => $value['block']
                    ),*/
                    array(
                      'TaxType' => 'OUTPUT',
                      'Name'    => 'Class',
                      'Option'  => $value['event_code']
                    )
                  )
                )
              )
            )
          )
        )
      );
  } else {
       $new_invoice = array(
        array(
          "Type" => "ACCREC",
          "Contact" => array(
            "ContactNumber"             => $value['contact_id'],
            'Name'                      => $value['display_name'],
            'AccountsRecievableTaxType' => 'OUTPUT',
            'FirstName'                 => $value['first_name'],
            'LastName'                  => $value['last_name']
          ),
          "Date"            => substr($value['receive_date'], 0, 10),
          "DueDate"         => substr($value['receive_date'], 0, 10),
          "Status"          => "SUBMITTED",
          "InvoiceNumber"   => $value['contribution_id'],
          "Reference"       => $cc . $value['event_code'] . " " . $value['display_name'],
          "CurrencyCode"    => /*'NZD'*/ CRM_Core_Config::singleton()->defaultCurrency,
          "LineAmountTypes" => "Inclusive",

          // "LineItems" => array(
          //   "LineItem" => array(
          //     array(
          //       "Description" => $value['event_code'] . " " . $value['display_name'],
          //       "Quantity"    => "1",
          //       "UnitAmount"  => $value['participant_fee_amount'],
          //       "AccountCode" => $accountType,
          //       "Tracking"    => array(
          //         "TrackingCategory" => array(

          //           array(
          //             'TaxType' => 'OUTPUT',
          //             'Name'    => 'Block / Project',
          //             'Option'  => $value['block']
          //           ),
          //           array(
          //             'TaxType' => 'OUTPUT',
          //             'Name'    => 'Class',
          //             'Option'  => $value['event_code']
          //           )
          //         )
          //       )
          //     )
          //   )
          // )
        )
      );
  }
  //print "line items = " . print_r($value['line_items'], true);
  if (is_array($value['line_items'])) {

    // For event based contributions, use Eileen's original code
    if (isset($value['participant_id']) and !empty($value['participant_id'])) {
      foreach ($value['line_items'] as $lineitem) {

        $new_invoice[0]['LineItems']['LineItem'][] = array(
          "Description" => $value['event_code'] . " " . $lineitem['display_name'],
          "Quantity" => "1",
          "UnitAmount" => $lineitem['participant_fee_amount'],
          "AccountCode" => $accountType,
          "Tracking" => array(
            "TrackingCategory" => array(
              /*
              array(
                'TaxType' => 'OUTPUT',
                'Name' => 'Block / Project',
                'Option' => $value['block']
              ),*/
              array(
                'TaxType' => 'OUTPUT',
                'Name' => /*'Class'*/t('Event Code'),
                'Option' => $value['event_code']
              )
            )
          )

        );
      }
    } else {

       // For other contributions, do this instead
       foreach ($value['line_items'] as $lineitem) {
          $new_invoice[0]['LineItems']['LineItem'][] = array(
            "Description" => $lineitem['label'],
            "Quantity"    => $lineitem['qty'],
            "UnitAmount"  => $lineitem['unit_price'],
            "AccountCode" => $accountType

          );
        }
      }
    } else {
      // if no line items, create one from the contribution details
      $new_invoice[0]['LineItems']['LineItem'][] = array(
        "Description" => $value['contribution_type'],
        "Quantity"    => 1,
        "UnitAmount"  => $value['total_amount'],
        "AccountCode" => $accountType
      );
    }

    //print_r($new_invoice);
    //echo "\n\n\n";
    $invoice_result = @drupal_static('civixero')->Invoices($new_invoice);
    //print_r($invoice_result);

    if ($errors = civixero_validate_response($invoice_result)) {
        civiaccountsync_error(t(
            "The following errors occurred creating an invoice in Xero for @this_contact (cid @cid), contribution id @contribution_id: <br />" .
            '<pre>' . print_r($errors, true) . '</pre>',
            array(
                '@this_contact'    => $value['display_name'],
                '@cid'             => $value['contact_id'],
                '@contribution_id' => $value['contribution_id']
            )
        ));
        return false;
    }

    drupal_write_record('cas_invoices', $objref = (object)array(
        'acc_iid'        => $invoice_result['Id'],
        'civicrm_iid'    => $value['contribution_id'],
        'status'         => $value['contribution_status_id'],
        'civicrm_amount' => $value['total_amount'],
        'acc_status'     => 2,
        'acc_last_mod'   => time(),
        'acc_amount'     => $value['total_amount'],
        'acc_cid'        => $invoice_result['Invoices']['Invoice']['Contact']['ContactID']
    ));

    // TODO: delete from cas_pending - IMPORTANT!! :)

    civiaccountsync_message(t(
        "Successfully created invoice in Xero for @that_contact (cid: @cid, contribution id: @contribution_id)",
        array(
            '@that_contact'    => $value['display_name'],
            '@cid'             => $value['contact_id'],
            '@contribution_id' => $value['contribution_id']
        )
    ));

    return true;

}

function civixero_validate_response($responseArray) {

    $message = '';
    $errors  = array();

    // comes back as a string for oauth errors
    if (is_string($responseArray)) {
        foreach (explode('&', $responseArray) as $response_item) {
            $keyval   = explode('=', $response_item);
            $errors[$keyval[0]] = urldecode($keyval[1]);
        }
        return $errors;
    }

    if (is_array($responseArray['Elements']['DataContractBase']['ValidationErrors'])) {
      foreach ($responseArray['Elements']['DataContractBase']['ValidationErrors'] as $key => $value) {
        // we have a situation where the validation errors are an array of errors
        // original code expected a string - not sure if / when that might happen
        // this is all a bit of a hackathon @ the moment
        if (is_array($value[0])) {
          foreach ($value as $errorMessage) {
            if (trim($errorMessage['Message']) == 'Account code must be specified') {
              return array(
                'You need to set up the account code'
              );
            }
            $message .= " " . $errorMessage['Message'];
          }
        }
        else { // single message - string
          $message = $value['Message'];
        }
        switch (trim($message)) {
          case "The Contact Name already exists. Please enter a different Contact Name.":
            $contact = $responseArray['Elements']['DataContractBase']['Contact'];
            $message .= "<br>contact ID is " . $contact['ContactNumber'];
            $message .= "<br>contact name is " . $contact['Name'];
            $message .= "<br>contact email is " . $contact['EmailAddress'];
            break;
          case "The TaxType field is mandatory Account code must be specified":
            $message = "Account code needs setting up";
        }
        $errors[] = $message;
      }
    }
    return is_array($errors) ? $errors : false;
}
