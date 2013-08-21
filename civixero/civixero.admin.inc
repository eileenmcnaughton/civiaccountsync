<?php
// $Id$

function civixero_settings_form ($form_state = NULL){
    
    $form['civixero_key']= array(
        '#type'          => 'textfield',
        '#title'         => t('Xero Key'),
        '#description'   => t('Please enter your Xero key'),
        '#default_value' => variable_get('civixero_key', ""),
    );

    $form['civixero_secret']= array(
        '#type'          => 'textfield',
        '#title'         => t('Secret Key'),
        '#description'   => t('Please enter your Xero secret key'),
        '#default_value' => variable_get('civixero_secret', ""),
    );

    $form['civixero_publiccert']= array(
        '#type'          => 'textfield',
        '#title'         => t('Public Certificate'),
        '#description'   => t('Please enter the full path to your public certificate'),
        '#default_value' => variable_get('civixero_publiccert', ""),
    );

    $form['civixero_privatekey']= array(
        '#type'          => 'textfield',
        '#title'         => t('Private Key'),
        '#description'   => t('Please enter the full path to your private key'),
        '#default_value' => variable_get('civixero_privatekey', ""),
    );

    $form['civixero_dayzero'] = array(
        '#type'          => 'date',
        '#title'         => t('Day Zero'),
        '#description'   => t('CiviXero will never push or pull invoices before this date'),
        '#default_value' => variable_get('civixero_dayzero', 0),
    );

    $form['advanced'] = array(
        '#type'        => 'fieldset',
        '#title'       => t('Advanced'),
        '#description' => '<strong>' . t('Important') . ':</strong> ' . t('Do not alter these settings unless you\'re sure you know what you what you\'re doing'),
        '#collapsible' => true,
        '#collapsed'   => true
    );

    $form['advanced']['civixero_lastxerocontactget']= array(
        '#type'          => 'date',
        '#title'         => t('Last Contact Sync from Xero'),
        '#description'   => t('Date of last contact sync from Xero'),
        '#default_value' => variable_get('civixero_lastxerocontactget', ""),
    );

    $form['advanced']['civixero_lastcivicontactput']= array(
        '#type'          => 'date',
        '#title'         => t('Last Contact Sync to Xero'),
        '#description'   => t('Date of last contact sync to Xero'),
        '#default_value' => variable_get('civixero_lastcivicontactput', ""),
    );

    $form['advanced']['civixero_lastxeroinvoiceget']= array(
        '#type'          => 'date',
        '#title'         => t('Last Invoice Sync from Xero'),
        '#description'   => t('Date of last invoice from Xero retrieval'),
        '#default_value' => variable_get('civixero_lastxeroinvoiceget', ""),
    );

    $form['civixero_groupxerocustomers']= array(
        '#type'          => 'select',
        '#title'         => t('Add Contacts to Group'),
        '#description'   => t('Add Xero customers to this CiviCRM group when created'),
        '#default_value' => variable_get('civixero_groupxerocustomers', ""),
        '#options'       => _civixero_get_groups(),
    );

    $form['civixero_automaticallymatchcontact']= array(
        '#type'          => 'checkbox',
        '#title'         => t('Automatically Match Contact'),
        '#description'   => t('If a Xero contact has a single match in Civi, create link automatically'),
        '#default_value' => variable_get('civixero_automaticallymatchcontact', 0),
    );

    #TODO Harden private key location + set defaults
    return system_settings_form($form);
}


// Retrieve all groups from CiviCRM
function _civixero_get_groups() {

    civicrm_initialize(true);
    require_once 'api/api.php';
    $result = civicrm_api('Group', 'get', array(
        'version' => 3
    ));

    if (!$result['is_error']) {
        $groups = array();
        foreach ($result['values'] as $group)
            $groups[$group['id']] = $group['title'];
        if (empty($groups))
            $groups = array(0 => t('No groups found'));
        return $groups; 
    } else {
        drupal_set_message('Unable to fetch groups from CiviCRM because ' . $result['error_message'], 'error');
        return array();
    }

}
