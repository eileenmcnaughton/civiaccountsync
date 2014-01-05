<?php

// $Id$
function civiaccountsync_form_contact(&$form_state) {
  module_load_include('inc', 'civiaccountsync', 'civiaccountsync.utils');

  // can get cid from url, or will default to next unmatched
  if (! $cas_cid = arg(3)) {
    $cas_cid = civiaccountsync_get_nextUnmatchedContact();
  }

  if (! $cas_cid) {
    drupal_set_message(t('There are currently no unmatched contacts to sync'));
    return;
  }

  // load contact data for the unmatched contact
  if (! $data = db_select('cas_contacts', 'c')->
    fields('c')->
    condition('cas_cid', $cas_cid, '=')->
    execute()->
    fetchObject()) {
    drupal_set_message(t('Unable to lookup contact data'), 'error');
    return;
  }

  $form['name'] = array(
    '#type' => 'item',
    '#title' => t('Name in Accounts'),
    '#markup' => $data->acc_full_name . ($data->acc_primary_email ? ' - ' . $data->acc_primary_email : '')
  );

  $form['sync_action'] = array(
    '#title' => t('Action to take with this contact'),
    '#type' => 'radios',


    '#options' => array(


      'create' => t('Create this contact in CiviCRM'),


      'exclude' => t('Exclude this contact from CiviCRM')
    )

    ,


    '#attributes' => array(


      'onchange' => array(
        "jQuery('.creation-params').animate({height:'toggle', opacity:'toggle'},500);"
      )
    )

    ,


    '#default_value' => 'create'
  )

  ;


  $form['do_not_email'] = array(


    '#title' => t('Set do not e-mail'),


    '#type' => 'checkbox',


    '#description' => t('Create option only Set contact to not receive e-mails from CiviCRM'),


    '#options' => array(
      '0' => 'No',
      1 => 'Yes'
    ),


    '#prefix' => '<div class="creation-params">'
  )

  ;


  $form['is_opt_out'] = array(


    '#title' => t('Set opt out as if user had opted out of CiviMails'),


    '#type' => 'checkbox',


    '#description' => t('Create option only contact to not receive CiviMails from CiviCRM'),


    '#options' => array(
      '0' => 'No',
      1 => 'Yes'
    )
  )

  ;


  $form['contact_type'] = array(


    '#title' => t('Contact type'),


    '#type' => 'select',


    '#options' => array(


      'Individual' => 'Individual',


      'Organization' => 'Organization'
    )

    ,


    '#default_value' => 'Individual'
  )

  ;


  $form['cas_cid'] = array(


    '#type' => 'value',


    '#value' => $cas_cid
  )

  ;


  $form['plugin'] = array(


    '#type' => 'value',


    '#value' => $data->plugin
  )

  ;


  $form['first_name'] = array(


    '#type' => 'value',


    '#value' => $data->acc_first_name
  )

  ;


  $form['last_name'] = array(


    '#type' => 'value',


    '#value' => $data->acc_last_name
  )

  ;


  $form['email'] = array(


    '#type' => 'value',


    '#value' => $data->acc_primary_email
  )

  ;


  $form['additional_data'] = array(


    '#type' => 'value',


    '#value' => $data->additional_data
  )

  ;


  $form['display_name'] = array(


    '#type' => 'value',


    '#value' => $data->acc_full_name
  )

  ;


  $form['civicrm_cid'] = array(


    '#type' => 'textfield',


    '#title' => t('CiviCRM ID'),


    '#description' => t('Specify Civi ID (provide a number)'),


    '#default_value' => $data->civi_suggested_contact_id,


    '#suffix' => '</div>'
  )

  ;


  $form['external_identifier'] = array(


    '#type' => 'value',


    '#value' => $data->acc_cid
  )

  ;


  $form['submit'] = array(


    '#type' => 'submit',


    '#value' => t('Add Contact')
  )

  ;


  return $form;
}
function civiaccountsync_form_contact_submit($form, &$form_state) {
  switch ($form_state['values']['sync_action']) {
    // 'Create contact in CiviCRM' was selected ..
    case 'create':
      if (! module_exists('civicrm')) {
        return;
      }
      civicrm_initialize(true);
      $params = array(


        'version' => 3,


        'first_name' => $form_state['values']['first_name'],


        'last_name' => $form_state['values']['last_name'],


        'display_name' => $form_state['values']['display_name'],


        'email' => $form_state['values']['email'],


        'do_not_email' => $form_state['values']['do_not_email'],


        'is_opt_out' => $form_state['values']['is_opt_out'],


        'contact_type' => $form_state['values']['contact_type'],


        'external_identifier' => str_replace('-', '', $form_state['values']['external_identifier'])
      )

      ;


      if ($params['contact_type'] == 'Organization') {


        $params['organization_name'] = $params['display_name'];
      }


      // Attempt to create contact in Civi


      $result = civicrm_api('contact', 'create', $params);


      // no error ..


      if (! $result['is_error']) {


        // API returns no error ..


        // Update internal contacts table with contact_id


        db_update('cas_contacts')->


        fields(array(


          'civicrm_cid' => $result['id']
        )

        )->


        condition('cas_cid', $form_state['values']['cas_cid'], '=')->


        execute();


        // Run hook_civiaccountsync_post_add_contact in the plugin module that
        // downloaded this contact


        module_invoke($form_state['values']['plugin'], 'civiaccountsync_post_add_contact', $result['id'], $form_state['values']['additional_data']);


        civiaccountsync_message(


        t(


        'Contact ' . ($params['contact_type'] == 'Individual' ? '@firstname @lastname' : '@display_name') . ' was successfully created in CiviCRM.',


        array(


          '@firstname' => $params['first_name'],


          '@lastname' => $params['last_name'],


          '@display_name' => $params['display_name']
        )

        )


        )


        ;


        // error ..
      }
      else {


        civiaccountsync_error(


        t(


        'Unable to create contact in CiviCRM for @firstname @lastname because @excuse',


        array(


          '@firstname' => $params['first_name'],


          '@lastname' => $params['last_name'],


          '@excuse' => $result['error_message']
        )

        )


        )


        ;
      }


      break;


    case 'exclude':


      // Update civi_ignore status in internal contacts table


      db_update('cas_contacts')->


      fields(array(


        'civi_ignore' => 1
      )

      )->


      condition('cas_cid', $form_state['values']['cas_cid'], '=')->


      execute();


      break;
  }


  $form_state['redirect'] = 'admin/cas/contact/' . civiaccountsync_get_nextUnmatchedContact();
}


/*
 * Form to manage the account sync process
 */

function civiaccountsync_form_dashboard() {
  module_load_include('inc', 'civiaccountsync', 'civiaccountsync.plugin');
  drupal_set_title(t('CiviAccountSync Dashboard'));
  if (! $plugins = civiaccountsync_get_plugins()) {
    drupal_set_message(t('CiviAccountSync is unable to locate any installed plugins.'), 'error');
    return;
  }

  $scheduler_intervals = array(
    1 => t('Hour'),
    2 => '2 ' . t('Hours'),
    3 => '3 ' . t('Hours'),
    6 => '6 ' . t('Hours'),
    12 => '12 ' . t('Hours'),
    24 => t('Day'),
    48 => '2 ' . t('Days'),
    96 => '4 ' . t('Days'),
    (7 * 24) => t('Week'),
    (14 * 24) => t('Fortnight'),
    (28 * 24) => t('Month')
  )
  ;

  $current_config = variable_get('civiaccountsync_sync_job_config', array());

  $form['plugins'] = array(
    '#type' => 'fieldset',
    '#title' => t('Accounts Package'),
    '#collapsible' => false
  )

  ;
  $form['plugins']['plugins'] = array(
    '#type' => 'radios',
    '#options' => $plugins,
    '#default_value' => isset($current_config['#dashboard_selected_plugin']) ? $current_config['#dashboard_selected_plugin'] : key($plugins),
    '#attributes' => array(
      'onchange' => "jQuery('.plugin-container').hide(); jQuery('.' + this.value + '-container').show();"
    )
  )
  ;


  $last_run_info = variable_get('civiaccountsync_last_run_info', array());
  foreach ($plugins as $module_name => $human_name) {
    if (! $capabilities = CiviAccountSyncPlugin::capabilities($module_name)) {
      drupal_set_message(t('Unable to query capabilities for @name', array(
        '@name' => $human_name
      )), 'error');
      continue;
    }


    $options = array();


    $default_values = array();


    if (! isset($current_config[$module_name]))
      $current_config[$module_name] = array();
    foreach ($capabilities as $function => $capability) {
      $options[$function] = array(
        'task' => $capability,
        'lastrun' => isset($last_run_info[$module_name][$function]) ? date('l jS M Y \a\t g.ia', $last_run_info[$module_name][$function]) : t('Never')
      )
      ;

     if (isset($current_config[$module_name][$function]) and $current_config[$module_name][$function])
        $default_values[$function] = true;
    }
    $form[$module_name . '-container'] = array(
      '#markup' => '<div class="' . $module_name . '-container plugin-container">'
    )

    ;

    $form[$module_name . '-settings'] = array(
      '#type' => 'tableselect',
      '#header' => array(
        'task' => t('Task'),
        'lastrun' => t('Last Run')
      )
      ,
      '#options' => $options,
      '#default_value' => $default_values,
      '#multiple' => true
    )
    ;

    $form[$module_name . '-scheduler'] = array(
      '#type' => 'fieldset',
      '#title' => t('Scheduler'),
      '#collapsible' => false
    )
    ;

    $scheduler_value = 'immediate';
    if (isset($current_config[$module_name]['#scheduler']) and $current_config[$module_name]['#scheduler']['enabled'])
      $scheduler_value = 'schedule';
      $form[$module_name . '-scheduler'][$module_name . '-scheduler-action'] = array(
      '#type' => 'radios',
      '#options' => array(
        'immediate' => t('Perform these tasks now'),
        'schedule' => t('Schedule these tasks')
      ),/*
      '#attributes' => array(
        'onchange' => array(
          "jQuery('.{$module_name}-scheduler-params').animate({height:'toggle', opacity:'toggle'},500);"
        ),
        'class' => array(
          'scheduler-action',
          $module_name . '-scheduler-select'
        )
      ),*/
      '#default_value' => $scheduler_value
    )
    ;


    $form[$module_name . '-scheduler'][$module_name . '-scheduler-interval'] = array(


      '#type' => 'select',


      '#prefix' => '<div class="' . $module_name . '-scheduler-params" style="padding-top:5px;">',


      '#suffix' => '</div>',


      '#field_prefix' => '<span style="font-size:0.9em;">' . t('Every') . '</span>',


      '#options' => $scheduler_intervals,


      '#default_value' => isset($current_config[$module_name]['#scheduler']) ? $current_config[$module_name]['#scheduler']['interval'] : key($scheduler_intervals)
    )

    ;


    $form[$module_name . '-container-end'] = array(
      '#markup' => '</div>'
    );

    // hide / show scheduler interval based on scheduler mode

/*
    drupal_add_js("jQuery(document).ready(function($) { if ($('.{$module_name}-scheduler-select input[type=radio]:checked').val() == 'immediate') $('.{$module_name}-scheduler-params').hide(); else $('.{$module_name}-scheduler-params').show(); });", 'inline');
*/
  }


  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Submit'
  );
  /* hide all available tasks except from the currently selected plugin
  drupal_add_js("jQuery(document).ready(function($) { $('.plugin-container').hide(); $('.' + $('.form-item-plugins input[type=radio]:checked').val() + '-container').show(); });",
    'type',
    'footer'
  );
  */

  return $form;
}


/*
 * implement form submit to process actions
 */

function civiaccountsync_form_dashboard_submit($form, &$form_state) {
  module_load_include('inc', 'civiaccountsync', 'civiaccountsync.utils');
  $job_config = array();
  $plugins = civiaccountsync_get_plugins();
  // save dashboard values

  foreach (array_keys($plugins) as $plugin) {
    $job_config[$plugin] = array();
    foreach (array_keys(civiaccountsync_get_plugin_capabilities($plugin)) as $capability) {
      $job_config[$plugin][$capability] =
      (int) (isset($form_state['values'][$plugin . '-settings'][$capability]) and $form_state['values'][$plugin . '-settings'][$capability] ? 1 : 0);
      $job_config[$plugin]['#scheduler'] = array(
      'enabled' => (int) ($form_state['values'][$plugin . '-scheduler-action'] == 'immediate' ? 0 : 1),
      'interval' => $form_state['values'][$plugin . '-scheduler-interval']
    );
    }
  }


  // remember this too, so we can automatically select it next time


  $job_config['#dashboard_selected_plugin'] = $form_state['values']['plugins'];

  // save our sync job configuration


  variable_set('civiaccountsync_sync_job_config', $job_config);

  // If the currently selected plugin has 'immediate' set, run sync job for that
  // plugin now.


  if ($form_state['values'][$form_state['values']['plugins'] . '-scheduler-action'] == 'immediate') {

    // Make a note of the number of pending contacts before and after doing
    // that, so we can
    // work out if + how many contacts were downloaded
    $pending_contacts_before = civiaccountsync_count_pending_contacts();

    civiaccountsync_do_sync_job(false);
    $pending_contacts_after = civiaccountsync_count_pending_contacts();

    if (($contacts_downloaded = ($pending_contacts_after - $pending_contacts_before)) > 0) {
      drupal_set_message(t(
        '@some contacts were downloaded.',
        array(
          '@some' => $contacts_downloaded
        )
      ));
      drupal_goto('admin/cas/contact/' . civiaccountsync_get_nextUnmatchedContact());
    }
    else {
      drupal_set_message(t('Configuration saved'));
    }
  }
  else {
    drupal_set_message(t('Configuration saved'));
  }
}