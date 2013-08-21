<?php

/**
 *
 * Expose a Drush command for interacting with CiviAccountSync
 * Primarily intended to aid plugin developers in testing plugin functionality, but may also be of use to some end users
 *
 * AndyW, Circle Interactive 2013
 *
 */

/**
 * Implementation of hook_drush_command()
 */
function civiaccountsync_drush_command() {
    
    $items = array();
    $eol   = PHP_EOL;

    $items['civiaccountsync'] = array (
        
        'aliases' => array(
            'cas'
        ),

        'description' => 'Interact with CiviAccountSync via the command line.' . $eol . 
                         'Intended to aid plugin developers in testing plugin functionality, but may also be of use to end users.' . $eol . $eol .
                         'Usage:' . $eol . $eol .
                         '     drush civiaccountsync <global-op>' . $eol .
                         '     drush civiaccountsync <plugin> <plugin-op>' . $eol .
                         '     drush cas <global-op>' . $eol . 
                         '     drush cas <plugin-name> <plugin-op>',

        'arguments' => array(
            'plugin_name_or_global_op' => 'Plugin to use, or global op such as add-all-contacts',
            'plugin_op'                => 'The id of the views_data_export display to execute on the view'
        ),

        'options' => array(
            'contact-type'    => 'For add-all-contacts: Specify the contact type to add as (default: Organization)',
            'do-not-contact'  => 'For add-all-contacts: do_not_contact setting on added contacts (default: 0)',
            'is-opt-out'      => 'For add-all-contacts: opt_out setting on added contacts (default: 0)'
        ),

        'examples' => array(
            'drush civiaccountsync add-all-contacts --contact-type=individual' => 'Add all downloaded contacts as contact type Individual',
            'drush civiaccountsync run-scheduled-jobs'                         => 'Manually run all scheduled tasks, the equivalent of running cron',
            'drush civiaccountsync plugins'                                    => 'List currently available plugins',
            'drush cas civixero add-all-contacts --contact-type=individual'    => 'Add all contacts downloaded by Civixero as contact type Individual',
            'drush cas civixero capabilities'                                  => 'List capabilities of the Civixero plugin - you can run any of these individually, see below:',
            'drush cas civixero contact-push'                                  => 'Run the Civixero plugin\'s contact-push task',
            'drush cas civixero invoice-pull'                                  => 'Run the Civixero plugin\'s invoice-pull task',
        ),

        'drupal dependencies' => array(
            'civiaccountsync'
        ),

        'core' => array('7'),
    
    );

    return $items;

}

/**
 * Implementation of hook_drush_help().
 */
function civiaccountsync_drush_help($section) {
    
    switch ($section) {
        case 'drush:civiaccountsync':
            return dt("This command can be used to list and run jobs provided by CiviAccountSync plugins, and general CiviAccountSync tasks.");
    }

}

/**
 * Command callback
 */
function drush_civiaccountsync($plugin_name_or_global_op, $plugin_op = null) {

    module_load_include('inc', 'civiaccountsync', 'civiaccountsync.utils');

    $sync_info = &drupal_static('civiaccountsync');
    $sync_info->is_drush = true;

    // Global op mode
    if (is_null($plugin_op)) {
        
        switch ($global_op = $plugin_name_or_global_op) {
            
            case 'add-all-contacts':

                // Add all pending contacts
                civiaccountsync_add_all_contacts(array(
                    'contact_type'   => ucfirst(drush_get_option('contact-type', 'Organization')),
                    'do_not_contact' => drush_get_option('do-not-contact', 0),
                    'is_opt_out'     => drush_get_option('is-opt-out', 0)
                ));
                break;
            
            case 'run-scheduled-jobs':

                // Run any scheduled jobs that are due to run
                civiaccountsync_do_sync_job();
                break;
            
            case 'plugins':

                // List available plugins
                drush_print();
                $rows = array(
                    array(t('Plugin name'), t('Human-readable name'), t('Description')),
                    array('', '', ''),
                );
                foreach (civiaccountsync_get_plugins() as $module_name => $human_name) {
                    $info   = system_get_info('module', $module_name);
                    $rows[] = array($module_name, $human_name, $info['description']);
                }
                drush_print_table($rows, true);
                break;

            default:
                
                drush_log(dt("Unrecognized plugin or global operation: '!argument'", array('!argument' => $global_op)), 'error');
        
        }

    // Plugin op mode      
    } else {

        module_load_include('inc', 'civiaccountsync', 'civiaccountsync.plugin');
        
        $plugin_name = $plugin_name_or_global_op;
        $plugins     = civiaccountsync_get_plugins();

        if (!isset($plugins[$plugin_name])) {
            civiaccountsync_error(t('Unable to find plugin: @plugin', array('@plugin' => $plugin_name)));
            return;
        }

        $plugin = new CiviAccountSyncPlugin($plugin_name);
        if (!$plugin->is_initialized()) 
            return;

        switch ($plugin_op) {
            
            case 'add-all-contacts':

                // Add all contacts downloaded by the specified plugin
                civiaccountsync_add_all_contacts(
                    array(
                        'contact_type'   => ucfirst(drush_get_option('contact-type', 'Organization')),
                        'do_not_contact' => drush_get_option('do-not-contact', 0),
                        'is_opt_out'     => drush_get_option('is-opt-out', 0)
                    ),
                    $plugin_name
                );
                break;

            case 'capabilities':
                
                // List plugin's capabilities
                drush_print(PHP_EOL . t('Listing capabilities for \'@plugin\':', array('@plugin' => $plugin_name)) . PHP_EOL);
                $rows = array(
                    array(t('Name'), t('Description')),
                    array('', '', ''),
                );
                
                foreach ($plugin->capabilities() as $machine_name => $human_name) {
                    // replace '_' with '-' in machine name and remove the plugin name prefix
                    $machine_name = implode('-', array_slice(explode('_', $machine_name), 1));
                    $rows[]       = array($machine_name, $human_name); 
                }

                drush_print_table($rows, true);
                break;

            default:

                $last_run_info       = variable_get('civiaccountsync_last_run_info', array());
                $num_contacts_before = civiaccountsync_count_pending_contacts();

                // Run the plugin's specified job
                $plugin_op = str_replace('-', '_', $plugin_op);
                $plugin->$plugin_op();

                // Update last run info
                $last_run_info[$plugin_name][$plugin_name . '_' . $plugin_op] = time();
                variable_set('civiaccountsync_last_run_info', $last_run_info);

                $num_contacts_added = (civiaccountsync_count_pending_contacts() - $num_contacts_before);
                if ($num_contacts_added)
                    civiaccountsync_message(t('@some contacts were downloaded', array('@some' => $num_contacts_added)));

                break;

        }


    }



}


