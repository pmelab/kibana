<?php
namespace Drupal\Kibana\services;

use GuzzleHttp\Client;
use GuzzleHttp\Psr7\Uri;
use Symfony\Component\HttpFoundation\Request as SymfonyRequest;
use GuzzleHttp\Psr7\Request as GuzzleRequest;
use Symfony\Bridge\PsrHttpMessage\Factory\HttpFoundationFactory;


class Proxy {

  /**
   * @var \GuzzleHttp\Psr7\Uri
   */
  protected $elasticsearch;

  /**
   * @var \GuzzleHttp\Client
   */
  protected $client;

  /**
   * @var \Symfony\Bridge\PsrHttpMessage\Factory\HttpFoundationFactory
   */
  protected $responseFactory;

  /**
   * @var \Drupal\Kibana\services\User
   */
  protected $user;

  /**
   * @var \Drupal\Kibana\services\Configuration
   */
  protected $config;

  public function __construct(Configuration $config, User $user) {
    $this->config = $config;
    $this->user = $user;
    $this->elasticsearch = new Uri($config->getElasticsearchUrl());
    $this->client = new Client();
    $this->responseFactory = new HttpFoundationFactory();
  }

  protected function doContextualize(SymfonyRequest $request) {
    return !($this->user->isInternal($request) || $this->user->isAdmin($request));
  }

  public function multiGet(SymfonyRequest $request) {
    if ($this->doContextualize($request)) {
      $content = json_decode($request->getContent(), TRUE);
      $content['docs'] = array_filter($content['docs'], function ($query) {
        return $query['_index'] == $this->config->getKibanaIndex();
      });
      $request = $this->requestWithContent($request, json_encode($content, JSON_UNESCAPED_SLASHES));
    }
    return $this->pipe($request);
  }

  protected function requestWithContent(SymfonyRequest $request, $content) {
    return new SymfonyRequest(
      $request->query->all(),
      $request->request->all(),
      $request->attributes->all(),
      $request->cookies->all(),
      $request->files->all(),
      $request->server->all(),
      $content
    );
  }

  public function multiSearch(SymfonyRequest $request) {
    if ($this->doContextualize($request)) {
      $lines = explode("\n", $request->getContent());
      $current_index = FALSE;
      $content = [];
      $uid = $this->user->getUid($request);

      foreach ($lines as $line) {

        $line = json_decode($line);

        if (isset($line->query)) {
          if (!$current_index) {
            continue;
          }
          if (!isset($line->query->bool)) {
            $old_query = json_decode(json_encode($line->query));
            $line->query->bool = new \stdClass();
            $line->query->bool->must = [];
            $line->query->bool->must[] = $old_query;
            unset($line->query->query_string);
          }
          foreach ($current_index as $index) {
            if ($ownership_field = $this->config->getOwnershipField($index)) {
              $line->query->bool->must[] = json_decode(json_encode([
                  'match' => [
                    $ownership_field => [
                      'query' => $uid,
                      'type' => "phrase",
                    ],
                  ]
                ])
              );
            }
          }
          $content[] = $line;
        }
        else {
          if (isset($line->index)) {
            $current_index = $line->index;
            $content[] = $line;
          }
        }
      }

      $content = implode("\n", array_map(function ($line) {
        return json_encode($line, JSON_UNESCAPED_SLASHES);
      }, $content));

      $request = $this->requestWithContent($request, $content . "\n");

    }
    return $this->pipe($request);
  }

  /**
   * Pipe a request to the kibana backend and return the response.
   */
  public function pipe(SymfonyRequest $request) {
    $uri = $this->elasticsearch->withPath($request->getPathInfo());

    $psrRequest = new GuzzleRequest(
      $request->getMethod(),
      $uri,
      $request->headers->all(),
      $request->getContent()
    );

    $psrResponse = $this->client->send($psrRequest, ['http_errors' => FALSE])
      ->withoutHeader('Transfer-Encoding');

    return $this->responseFactory->createResponse($psrResponse);
  }
}
