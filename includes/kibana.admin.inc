<?php

/**
 * System settings form for Kibana settings.
 */
function kibana_settings_form($form, &$form_state) {

  $form['kibana_es_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Elasticsearch Url'),
    '#required' => TRUE,
    '#default_value' => kibana_es_url(),
  );

  $form['kibana_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Kibana index'),
    '#required' => TRUE,
    '#default_value' => kibana_index(),
  );

  $form['kibana_status'] = array(
    '#type' => 'fieldset',
    '#title' => 'Elasticsearch component status',
    '#collapsible' => TRUE,
    'messages' => array(),
    'button' => array(
      '#weight' => 100,
      '#type' => 'submit',
      '#value' => t('Update elasticsearch components'),
      '#submit' => array('kibana_settings_form_setup'),
    ),
  );

  $client = kibana_elasticsearch_client();

  $messages = [];

  try {
    $client->ping();

    if ($client->indices()->exists(array('index' => kibana_index()))) {
      $messages[] = t('Kibana management index %index is available.', array(
        '%index' => kibana_index(),
      ));
    } else {
      $messages[] = t('Kibana management index %index <strong>not</strong> is available.', array(
        '%index' => kibana_index(),
      ));
    }

    $templates_in_sync = TRUE;
    foreach (kibana_get_index_templates() as $id => $template) {
      try {
        $client->indices()->getTemplate(array(
          'name' => $id,
        ));
      } catch (Exception $exc) {
        $templates_in_sync = FALSE;
      }
    }

    if ($templates_in_sync) {
      $messages[] = t('Index templates are synchronized with definitions.');
    }
    else {
      $messages[] = t('Index templates are <strong>not</strong> synchronized with definitions.');
    }

    $pipelines_in_sync = TRUE;

    foreach (kibana_get_pipelines() as $id => $pipeline) {
      try {
        $client->ingest()->getPipeline(array('id' => $id));
      } catch (Exception $exc) {
        $pipelines_in_sync = FALSE;
      }
    }

    if ($pipelines_in_sync) {
      $messages[] = t('Pipelines are synchronized with definitions.');
    }
    else {
      $messages[] = t('Pipelines are <strong>not</strong> synchronized with definitions.');
    }

  } catch (Exception $exc) {
    $messages[] = t('Elasticsearch server not available at %url.', array(
      '%url' => kibana_es_url(),
    ));
  }

  foreach ($messages as $msg) {
    $form['kibana_status']['messages'][] = array(
      '#markup' => '<p>' . $msg . '</p>',
    );
  }

  return system_settings_form($form);
}

function kibana_settings_form_setup($form, $form_state) {
  module_load_include('inc', 'kibana', 'includes/kibana.setup');
  kibana_elasticsearch_setup();
  drupal_set_message('Updated elasticsearch environment.');
}

function kibana_indices_overview() {
  $client = kibana_elasticsearch_client();

  $form['indices'] = array(
    '#theme' => 'table',
    '#header' => array(
      'index' => t('Index'),
      'status' => t('Status'),
      'documents' => t('Documents'),
      'aliases' => t('Aliases'),
      'actions' => t('Actions'),
    ),
    '#rows' => array(),
    '#empty' => t('No indices available.'),
  );

  foreach ($client->indices()->stats()['indices'] as $index => $stats) {
    // Don't allow modifications of the kibana management index.
    if ($index == kibana_index()) {
      continue;
    }
    $aliases = [];

    foreach ($client->indices()->get(array('index' => $index)) as $i) {
      $aliases = array_keys($i['aliases']);
    }

    $form['indices']['#rows'][$index] = array(
      'index' => $index,
      'status' => '',
      'documents' => $stats['total']['docs']['count'],
      'aliases' => implode(', ', $aliases),
      'actions' => theme('item_list', array(
        'attributes' => array(
          'class' => array('links', 'inline'),
        ),
        'items' => array(
          l(t('reindex'), 'admin/config/search/kibana/indices/' . $index . '/reindex'),
          l(t('edit'), 'admin/config/search/kibana/indices/' . $index . '/edit'),
          l(t('delete'), 'admin/config/search/kibana/indices/' . $index . '/delete'),
        ),
      )),
    );
  }

  return $form;
}

function kibana_index_edit_form($form, &$form_state) {
  $client = kibana_elasticsearch_client();
  $aliases = array();

  $indices = $client->indices()->get(array(
    'index' => $form_state['build_info']['args'][0],
  ));

  foreach ($indices as $index) {
    $aliases = array_keys($index['aliases']);
  }

  $form['aliases'] = array(
    '#type' => 'textarea',
    '#title' => t('Index aliases'),
    '#description' => t('Configure aliases for this index. One alias per line.'),
    '#default_value' => implode(PHP_EOL, $aliases),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  return $form;
}

function kibana_index_edit_form_validate($form, &$form_state) {
  $client = kibana_elasticsearch_client();
  $aliases = explode(PHP_EOL, str_replace(array("\r"), "", $form_state['values']['aliases']));
  $current_aliases = array();
  $index_name = $form_state['build_info']['args'][0];
  foreach ($client->indices()->get(array('index' => $index_name)) as $index) {
    $current_aliases = array_keys($index['aliases']);
  }


  foreach (array_diff($aliases, $current_aliases) as $alias) {
    if ($client->indices()->exists(array('index' => $alias))) {
      form_set_error('aliases', t('An index with name %alias already exists.', array(
        '%alias' => $alias,
      )));
    }

    if ($client->indices()->existsAlias(array('name' => $alias))) {
      form_set_error('aliases', t('An alias with name %alias already exists.', array(
        '%alias' => $alias,
      )));
    }
  }
}

function kibana_index_edit_form_submit($form, &$form_state) {
  $client = kibana_elasticsearch_client();
  $aliases = explode(PHP_EOL, str_replace(array("\r"), "", $form_state['values']['aliases']));
  $current_aliases = array();

  $index_name = $form_state['build_info']['args'][0];

  foreach ($client->indices()->get(array('index' => $index_name)) as $index) {
    $current_aliases = array_keys($index['aliases']);
  }

  foreach (array_diff($aliases, $current_aliases) as $alias) {
    $client->indices()->putAlias(array(
      'index' => $index_name,
      'name' => $alias,
    ));
  }

  foreach (array_diff($current_aliases, $aliases) as $alias) {
    $client->indices()->deleteAlias(array(
      'index' => $index_name,
      'name' => $alias,
    ));
  }

  $form_state['redirect'] = 'admin/config/search/kibana/indices';
}

function kibana_index_reindex_form($form, &$form_state) {
  $pipelines = array('__none__' => t('None'));
  foreach (kibana_get_pipelines() as $pipeline => $info) {
    $pipelines[$pipeline] = array_key_exists('description', $info) ? $info['description'] : $pipeline;
  }

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Index name'),
    '#description' => t('The name of the new index.'),
  );

  $form['pipeline'] = array(
    '#type' => 'select',
    '#title' => t('Pipeline'),
    '#description' => t('Attach a pipeline to process documents.'),
    '#options' => $pipelines,
    '#access' => count($pipelines) > 0,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  return $form;
}

function kibana_index_reindex_form_validate($form, &$form_state) {
  $client = kibana_elasticsearch_client();
  $name = $form_state['values']['name'];
  if ($client->indices()->exists(array('index' => $name))) {
    form_set_error('aliases', t('An index with name %name already exists.', array(
      '%name' => $name,
    )));
  }

  if ($client->indices()->existsAlias(array('name' => $name))) {
    form_set_error('aliases', t('An alias with name %name already exists.', array(
      '%name' => $name,
    )));
  }
}

function kibana_index_reindex_form_submit($form, &$form_state) {
  $client = kibana_elasticsearch_client();
  $body = array(
    'source' => array('index' => $form_state['build_info']['args'][0]),
    'dest' => array('index' => $form_state['values']['name']),
  );

  if ($form_state['values']['pipeline'] != '__none__') {
    $body['dest']['pipeline'] = $form_state['values']['pipeline'];
  }

  $client->reindex(array('body' => $body));
  $form_state['redirect'] = 'admin/config/search/kibana/indices';
}

function kibana_index_delete_form($form, &$form_state) {
  $index = $form_state['build_info']['args'][0];
  $title = t('Are you sure you want to delete index %index?', array(
    '%index' => $index,
  ));
  $description = t('This action will delete the elasticsearch index %index and all contained documents.', array(
    '%index' => $index,
  ));
  return confirm_form($form, $title, 'admin/config/search/kibana/indices', $description);
}

function kibana_index_delete_form_submit(&$form, &$form_state) {
  $index = $form_state['build_info']['args'][0];
  $client = kibana_elasticsearch_client();
  $client->indices()->delete(array('index' => $index));
  drupal_set_message(t('Deleted index %index.', array('%index' => $index)));
  $form_state['redirect'] = 'admin/config/search/kibana/indices';
}