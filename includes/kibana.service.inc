<?php

/**
 * Search API service that is supposed to care about indexing into
 * elasticsearch only.
 */
class SearchApiKibana extends SearchApiAbstractService {
  /**
   * @var \Elasticsearch\Client
   */
  protected $_client;

  /**
   * {@inheritdoc}
   */
  public function __construct(\SearchApiServer $server) {
    parent::__construct($server);
  }


  /**
   * {@inheritdoc}
   */
  public function indexItems(SearchApiIndex $index, array $items) {
    $indexed = [];
    foreach ($items as $id => $item) {
      try {

        $body = array_map(function ($field) {
          if ($field['type'] == 'tokens') {
            return implode(' ', array_map(function ($token) {
              return $token['value'];
            }, $field['value']));
          }
          return $field['value'];
        }, $item);

        $this->client()->index(array(
          'index' => $index->machine_name,
          'type' => $index->machine_name,
          'id' => $id,
          'body' => $body,
        ));

        $indexed[] = $id;

      } catch(Exception $exc) {
        watchdog('error', $exc->getMessage());
      }
    }
    return $indexed;
  }

  /**
   * {@inheritdoc}
   */
  public function deleteItems($ids = 'all', SearchApiIndex $index = NULL) {
    if (!$index) {
      return;
    }

    try {
      if ($ids == 'all') {
        $this->client()->indices()->delete(array(
          'index' => $index->machine_name
        ));
      }
      else if ($index && is_array($ids)) {
        foreach ($ids as $id) {
          $this->client()->delete(array(
            'index' => $index->machine_name,
            'type' => $index->machine_name,
            'id' => $id,
          ));
        }
      }
    } catch (\Elasticsearch\Common\Exceptions\Missing404Exception $exc) {
      // Ignore missing index exception. We want to delete it anyway.
    }
  }

  /**
   * {@inheritdoc}
   */
  public function search(SearchApiQueryInterface $query) {
    // Very incomplete implementation of the "search" method.
    // Always returns number of all indexed items. Doesn't really search.
    // This service is meant for indexing only.
    try {
      $result = $this->client()->count(array(
        'index' => $query->getIndex()->machine_name,
        'type' => $query->getIndex()->machine_name,
      ));
      return array(
        'result count' => $result['count'],
        'results' => []
      );
    } catch (\Elasticsearch\Common\Exceptions\Missing404Exception $exc) {
      return array(
        'result count' => 0,
        'results' => [],
      );
    }
  }

  /**
   * {@inheritdoc}
   */
  public function addIndex(SearchApiIndex $index) {
    $this->updateMapping($index);
  }

  /**
   * {@inheritdoc}
   */
  public function fieldsUpdated(SearchApiIndex $index) {
    return $this->updateMapping($index);
  }


  /**
   * Update the index mapping.
   */
  public function updateMapping(SearchApiIndex $index) {

    $properties = array_map(function ($field) {
      return $this->getFieldMapping($field);
    }, $index->getFields());

    // Delete the index and to rebuild it with new mappings afterwards.
    $this->deleteItems('all', $index);

    $this->client()->indices()->create(array(
      'index' => $index->machine_name,
      'body' => array(
        'mappings' => array(
          '_default_' => array(
            'properties' => $properties,
          ),
        ),
      ),
    ));

    return TRUE;
  }

  /**
   * Helper function. Get the elasticsearch mapping for a field.
   */
  protected function getFieldMapping($field) {
    $field_type = isset($field['real_type']) ? $field['real_type'] : $field['type'];

    $type = search_api_extract_inner_type($field_type);

    switch ($type) {
      case 'text':
        return array(
          'type' => 'string',
        );

      case 'uri':
      case 'string':
      case 'token':
        return array(
          'type' => 'string',
          'index' => 'not_analyzed',
        );

      case 'integer':
      case 'duration':
        return array(
          'type' => 'integer',
        );

      case 'boolean':
        return array(
          'type' => 'boolean',
        );

      case 'decimal':
        return array(
          'type' => 'float',
        );

      case 'date':
        return array(
          'type' => 'date',
          'format' => 'epoch_second',
        );


      case 'location':
        return array(
          'type' => 'geo_point',
          'lat_lon'=> TRUE,
        );

      default:
        return NULL;
    }
  }

}
