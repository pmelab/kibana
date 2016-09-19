<?php

/**
 * Ensure all required indices and components are available in elasticsearch.
 */
function kibana_elasticsearch_setup() {
  $client = kibana_elasticsearch_client();

  // Create the kibana management index.
  if (!$client->indices()->exists(array('index' => kibana_index()))) {
    $client->indices()->create(array('index' => kibana_index()));
  }

  // Install templates.
  foreach (kibana_get_index_templates() as $id => $template) {
    try {
      $client->indices()->putTemplate(array(
        'name' => $id,
        'body' => $template,
      ));
    } catch(Exception $exc) {
      drupal_set_message($exc->getMessage(), "error");
    }
  }

  // Install pipelines.
  foreach (kibana_get_pipelines() as $id => $pipeline) {
    try {
      $client->ingest()->putPipeline(array(
        'id' => $id,
        'body' => $pipeline,
      ));
    } catch (Exception $exc) {
      drupal_set_message($exc->getMessage(), "error");
    }
  }
}