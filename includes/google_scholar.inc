<?php

/**
 * @file
 * Module used to embed meta tags in the HEAD for use in indexing in Google
 * Scholar and Zotero.
 */

/**
 * Creates meta tags to be placed in the head of the HTML.
 *
 * @param AbstractObject $object
 *   A AbstractObject.
 *
 * @return array
 *   Associative array where the key is the name of the tag and the value is
 *   the content.
 */
function islandora_scholar_create_meta_tags($object) {
  // Need at least title, full name of at least one author, publication year.
  if (!isset($object['MODS'])) {
    return FALSE;
  }
  else {
    $tags = array();
    $tags['citation_author'] = array();
    $mods = $object['MODS']->content;
    $mods_xml = new SimpleXMLElement($mods);
    $mods_xml->registerXPathNamespace('mods', 'http://www.loc.gov/mods/v3');

    $title_results = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_title', '//mods:mods[1]/mods:titleInfo/mods:title'));
    $title = (string) reset($title_results);
    $subtitle_results = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_title_sub_title', '//mods:mods[1]/mods:titleInfo/mods:subTitle'));
    $subtitle = (string) reset($subtitle_results);

    if (!empty($title)) {
      if (!empty($subtitle)) {
        $tags['citation_title'] = "{$title}: {$subtitle}";
      }
      else {
        $tags['citation_title'] = $title;
      }
    }
    else {
      $tags['citation_title'] = 'No title';
    }
    $authorsxpath = variable_get('islandora_scholar_xpaths_authors_xpath', '//mods:mods[1]/mods:name/mods:role[mods:roleTerm = "author"]/..');
    foreach ($mods_xml->xpath($authorsxpath) as $name_xml) {
      $name_parts = array();
      $name_xml->registerXPathNamespace('mods', 'http://www.loc.gov/mods/v3');
      // Use Mods DisplayForm as a backup source for name.
      // Only taking the first result for displayForm.
      $name_displayForm = '';
      $mods_displayform = $name_xml->xpath('mods:displayForm');
      if (is_array($mods_displayform)) {
        $name_displayForm = array_shift($mods_displayform);
      }
      foreach ($name_xml->xpath('mods:namePart') as $name_part) {
        if ((string) $name_part != '') {
          // Strip periods off the end.
          $np = (string) rtrim($name_part, '.');
          if ($name_part['type'] == 'given') {
            // Middle name is also marked as given name in MODS.
            if (isset($name_parts['first_name'])) {
              $name_parts['first_name'] = $name_parts['first_name'] . ' ' . (string) $name_part;
            }
            else {
              $name_parts['first_name'] = (string) $name_part;
            }
          }
          if ($name_part['type'] == 'family') {
            $name_parts['last_name'] = $np;
          }
          if (!isset($name_part['type'])) {
            $name_parts['no_type'] = $np;
          }
        }
      }
      if (isset($name_parts['last_name']) && isset($name_parts['first_name'])) {
        $tags['citation_author'][] = $name_parts['last_name'] . ', ' . $name_parts['first_name'];
      }
      elseif ($name_displayForm != '') {
        $tags['citation_author'][] = $name_displayForm;
      }
      elseif (isset($name_parts['no_type'])) {
        $tags['citation_author'][] = $name_parts['no_type'];
      }
    }
    if (count($tags['citation_author']) == 0) {
      $tags['error'] = 'author'; // return FALSE;
    }

    $origin_date = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_origin_date', '//mods:originInfo/mods:dateIssued'));
    $part_date = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_part_date', '//mods:part/mods:date'));
    $related_date = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_related_date', '//mods:relatedItem[@type="host"]//mods:date'));
    $created_date = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_created_date', '//mods:originInfo/mods:dateCreated'));

    $date = NULL;
    if ($origin_date) {
      $date = (string) reset($origin_date);
    }
    elseif ($part_date) {
      $date = (string) reset($part_date);
    }
    elseif ($related_date) {
      $date = (string) reset($related_date);
    }
    elseif ($created_date) {
      $date = (string) reset($created_date);
    }
    else {
      $tags['error'] = 'date 1'; // return FALSE;
    }

    // Google requires dates in yyyy/mm/dd format or just the year. As dates suck
    // and we don't have a consistent structure of input we will try to parse it as a
    // w3cdtf formatted date string and format it the right way. If that does not work
    // just return the year for now.

    if ($date) {
      $date_string = islandora_scholar_parse_date_foryear($date);
      if ($date_string) {
        $tags['citation_publication_date'] = $date_string;
      }
      else {
        return FALSE;
      }
    }

    $genre = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_genre', '//mods:mods[1]/mods:genre'));

    $genre = strtolower((string) reset($genre));
    $host_title = '';

    switch ($genre) {
      case 'info:eu-repo/semantics/article': // Article / Letter to editor
      case 'info:eu-repo/semantics/bookPart': // Part of book or chapter of book
        $tmp_title = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_host_title', '//mods:relatedItem[@type="host"]//mods:title'));
        if ($tmp_title) {
          $tags['citation_book_title'] = (string) reset($tmp_title);
        }
        // No break for a reason!
      case 'info:eu-repo/semantics/other': // Abstract, Event
      case 'info:eu-repo/semantics/annotation': // Annotation
      case 'info:eu-repo/semantics/bookreview': // Book review
      case 'info:eu-repo/semantics/workingpaper': // Research Paper 
        $host_title = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_host_title', '//mods:relatedItem[@type="host"]//mods:title'));
        $host_tag = 'citation_journal_title';
        break;

      case 'info:eu-repo/semantics/book': // Book, Book editorial
      case 'info:eu-repo/semantics/report': // External research report
        $tags['citation_book_title'] = $tags['citation_title'];
        break;

     case 'info:eu-repo/semantics/doctoralthesis':
        $tags['citation_dissertation_name'] = 'PhD';
        break;

      default:
        $host_title = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_host_title', '//mods:relatedItem[@type="host"]//mods:title'));
        $host_tag = 'citation_book_title';
        break;
    }
    if ($host_title) {
      $tags[$host_tag] = (string) reset($host_title);
    }

    $conference_title = $mods_xml->xpath("//mods:name[@type='conference']/mods:namePart[not(@type)]");
    if ($conference_title) {
      $tags['citation_conference_title'] = (string) reset($conference_title);
    }

    $volume = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_volume', '//mods:mods[1]/mods:part/mods:detail[@type="volume"]/mods:number'));
    if ($volume) {
      $tags['citation_volume'] = (string) reset($volume);
    }

    $issn = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_issn', '//mods:identifier[@type="issn"]'));
    if ($issn) {
      $tags['citation_issn'] = str_replace(
          array("&", "=", ",", ";"),
          array('', '', '', ''),
          (string) reset($issn));
    }

    $isbns = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_isbn', '//mods:identifier[@type="isbn" or @type="eisbn"]'));
    if ($isbns) {
      $tags['citation_isbn'] = array_map(function($isbn) { return (string)$isbn; }, $isbns);
    }

    $issue = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_issue', '//mods:mods[1]/mods:part/mods:detail[@type="issue"]/mods:number'));
    if ($issue) {
      $tags['citation_issue'] = (string) reset($issue);
    }

    $start_page = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_start_page', '//mods:extent[@unit="page"]/mods:start'));
    if ($start_page) {
      $tags['citation_firstpage'] = (string) reset($start_page);
    }

    $end_page = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_end_page', '//mods:extent[@unit="page"]/mods:end'));
    if ($end_page) {
      $tags['citation_lastpage'] = (string) reset($end_page);
    }

    $online_date = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_online_date', '//mods:recordInfo/mods:recordCreationDate'));
    if ($online_date) {
      $date = (string) reset($online_date);
      $date_string = islandora_scholar_parse_date_foryear($date);
      if ($date_string) {
        // $tags['citation_online_date'] = $date_string;
      }
    }

    $doi = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_doi', '//mods:identifier[@type="doi"]'));
    if ($doi) {
      $tags['citation_doi'] = (string) reset($doi);
    }

    $handle = $mods_xml->xpath('/mods:mods/mods:identifier[@type="hdl"]');
    if ($handle) {
      $tags['citation_public_url'] = (string) reset($handle);
    }

    $language = $mods_xml->xpath('/mods:mods/mods:language/mods:languageTerm[@type="code"]');
    if ($language) {
      $tags['citation_language'] = (string) reset($language);
    }

    $patent = $mods_xml->xpath('/mods:mods/mods:note[@type="patentNumber"]');
    if ($patent) {
      $tags['citation_patent'] = (string) reset($patent);
      $tags['citation_patent_number'] = $tags['citation_patent'];
    }

    $seriestitle = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_collection-title', '/mods:nothing'));
    if ($seriestitle) {
      $tags['citation_series_title'] = (string) reset($seriestitle);
    }

    $publisher = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_publisher', '/mods:nothing'));
    if ($publisher) {
      $publisher = array_map(function($pub) { return (string)$pub; }, $publisher);
      $tags['citation_publisher'] = $publisher;
    }

    $abstract = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_abstract', '/mods:nothing'));
    if ($abstract) {
      $tags['citation_abstract'] = array_map(function($a) { return strip_tags((string)$a); }, $abstract);
    }

    $degree_grantor = $mods_xml->xpath(variable_get('islandora_scholar_xpaths_degree_grantor', '//mods:mods/mods:name[@type="corporate"][mods:role/mods:roleTerm = "Degree grantor"]/mods:namePart'));
    if ($degree_grantor) {
      $tags['citation_dissertation_institution'] = (string) reset($degree_grantor);
    }

    $keywords = $mods_xml->xpath('//mods:subject/mods:topic');
    if ($keywords) {
      $tags['citation_keywords'] = array_map(function($kw) { return strip_tags((string)$kw); }, $keywords);
    }

    if (in_array('islandora:compoundCModel', $object->models)) {
      module_load_include('inc', 'islandora_solr', 'includes/utilities');
      $qp = new IslandoraSolrQueryProcessor();
      $usedsolrfields = array('mods_accessCondition_type_custom_ms', 'mods_originInfo_encoding_w3cdtf_type_embargo_dateOther_mdt', 'PID');
      $parent_id = islandora_solr_lesser_escape($object->id);
      $relcomp = variable_get('islandora_solr_compound_relationship_field', 'RELS_EXT_isConstituentOf_uri_ms');
      $query = "$relcomp:($parent_id) OR $relcomp:(" . islandora_solr_lesser_escape('info:fedora/') . "$parent_id)";
      $qp->buildQuery("*:*");
      $qp->solrStart = 0;
      $qp->solrLimit = 100;
      $qp->solrParams['facet'] = 'false';
      $qp->solrParams['fq'] = array($query);
      $qp->solrParams['fl'] = implode(',', $usedsolrfields);
      $qp->executeQuery(FALSE);

      if (isset($qp->islandoraSolrResult['response']['numFound']) && $qp->islandoraSolrResult['response']['numFound'] > 0) {
        $today = date("Y-m-d");
        foreach ($qp->islandoraSolrResult['response']['objects'] as $solrobj) {
          if (isset($solrobj['solr_doc']['mods_accessCondition_type_custom_ms'])) {
            $add = FALSE;
            if (in_array('info:eu-repo/semantics/openAccess', $solrobj['solr_doc']['mods_accessCondition_type_custom_ms'])) {
              $add = TRUE;
            }
            elseif (in_array('info:eu-repo/semantics/embargoedAccess', $solrobj['solr_doc']['mods_accessCondition_type_custom_ms'])) {
              if (isset($solrobj['solr_doc']['mods_originInfo_encoding_w3cdtf_type_embargo_dateOther_mdt'][0])) {
                $date = $solrobj['solr_doc']['mods_originInfo_encoding_w3cdtf_type_embargo_dateOther_mdt'][0];
                if (strcmp($date, $today) < 0) {
                  $add = TRUE;
                } 
              }
            }
            if ($add) {
              $cid = $solrobj['PID'];
              $tags['citation_pdf_url'][] = url("access/$cid/view", array('absolute' => TRUE));
            }
          }
        }
      }
    }
    else if (isset($object['PDF'])) {
      $tags['citation_pdf_url'] = url("islandora/object/$object->id/datastream/PDF/view", array('absolute' => TRUE));
    }

    $tags['citation_abstract_html_url'] = url("islandora/object/$object->id/", array('absolute' => TRUE));
  }

  // Array filter so no tags are created for empty values!
  return array_filter($tags);
}

/**
 * Return the year portion of a date.
 *
 * @param string|array $date
 *   The date to parse.  If it is array we use the first element in the array
 *
 * @return null|string
 *   returns the year if the date was parsable and NULL otherwise
 */
function islandora_scholar_parse_date_foryear($date) {
  if (is_array($date)) {
    $date = (string) reset($date);
  }
  if (($datetime = DateTime::createFromFormat('Y/m/d', $date)) !== FALSE) {
    return $datetime->format('Y-m-d');
  }
  elseif (($datetime = DateTime::createFromFormat('Y-m-d', $date)) !== FALSE) {
    return $datetime->format('Y-m-d');
  }
  elseif (($datetime = DateTime::createFromFormat('y/m/d', $date)) !== FALSE) {
    return $datetime->format('Y-m-d');
  }
  elseif (($datetime = DateTime::createFromFormat('y-m-d', $date)) !== FALSE) {
    return $datetime->format('Y-m-d');
  }
  elseif (($datetime = DateTime::createFromFormat('Y', $date)) !== FALSE) {
    return $datetime->format('Y');
  }
  else {
    // Date does not appear to be in a format we know of. Return it just in case.
    // TODO: should we log this?
    return $date;
  }
}

/**
 * Adds the meta tags to the HEAD of the html document.
 *
 * @param array $tags
 *   An associate array containing the name => content of the meta tags.
 */
function islandora_scholar_embed_tags($tags) {
  $weight = 1000;
  if ($tags != FALSE) {
    foreach ($tags as $name => $content) {
      if (is_array($content)) {
        foreach ($content as $key => $nested_val) {
          if ($name == 'citation_author') {
            $weight++;
          }
          $element = array(
            '#tag' => 'meta',
            '#attributes' => array(
              'name' => $name,
              'content' => htmlspecialchars($nested_val),
            ),
            '#weight' => $weight,
          );
          drupal_add_html_head($element, $key . '_' . $nested_val);
        }
      }
      else {
        $element = array(
          '#tag' => 'meta',
          '#attributes' => array(
            'name' => $name,
            'content' => htmlspecialchars($content),
          ),
        );
        drupal_add_html_head($element, $name);
      }
    }
  }
}
