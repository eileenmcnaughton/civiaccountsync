<?php

function civiaccountsync_dummy_capabilities() {
    return array(
        'civiaccountsync_dummy_do_nothing'        => t("Don't do anything at all"),
        'civiaccountsync_dummy_make_tea'          => t('Go and make a cup of tea'),
        'civiaccountsync_dummy_do_something_else' => t('Do something else'),
        'civiaccountsync_dummy_go_home'           => t('Go home'),
        'civiaccountsync_dummy_buy_balloons'      => t('Go and buy some balloons. Because balloons are good.'),
        'civiaccountsync_dummy_sleep'             => t('Go to bed'),
    );
}

/*
 * Init function will be called if defined - use this to perform pre-sync setup tasks,
 * such as initializing objects / connections
 */
function civiaccountsync_dummy_init() {
    civiaccountsync_message('Initializing myself now.');
}

/**
 * Handlers for each of the capabilities defined above
 * @param  last_run (timestamp) - time this job was last run
 * @return boolean indicating success or failure
 */
function civiaccountsync_dummy_do_nothing($last_run) {
    civiaccountsync_message('Doing nothing');
    return true;
}

function civiaccountsync_dummy_make_tea($last_run) {
    civiaccountsync_message('About to make tea. Standby.');
    return true;
}

function civiaccountsync_dummy_do_something_else($last_run) {
    civiaccountsync_message('Doing something else now.');
    return true;
}

function civiaccountsync_dummy_go_home($last_run) {
    civiaccountsync_message('Going home now.');
    return true;
}

function civiaccountsync_dummy_buy_balloons($last_run) {
    civiaccountsync_message('Balloon status = OK');
    return true;
}

function civiaccountsync_dummy_sleep($last_run) {
    civiaccountsync_message('Going to bed now.');
    return true;
}
