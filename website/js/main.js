requirejs.config({
    baseUrl: "",
    config: {
        'oscar': {
// 			url: "http://localoscar/oscar",
//			url: "https://oscardev.fmi.uni-stuttgart.de/oscar",
            url: "http://localhost/oscar",
			//the following variables have to match the ones in your server config (or should be smaller)
			maxFetchItems: 2000,
			maxFetchShapes: 2000,
			maxFetchIdx: 1000
		}
    },
    paths: {
        "fuzzysort" : "vendor/fuzzysort/fuzzysort",
        "jquery": "vendor/jquery/jquery.min",
        "jqueryui": "vendor/jquery-ui/jquery-ui.min",
        "bootstrap": "vendor/twitter-bootstrap/js/bootstrap.min",
		"typeahead" : "vendor/typeahead/typeahead.jquery",
		"bloodhound" : "vendor/typeahead/bloodhound",
        "leaflet": "vendor/leaflet/leaflet-src",
        "leafletCluster": "vendor/leaflet-markercluster/leaflet.markercluster-src",
		"leafletBing" : "vendor/leaflet-bing-layers/leaflet-bing-layer",
		"fetch-jsonp" : "vendor/leaflet-bing-layers/fetch-jsonp",
		"awesomeMarkers" : "vendor/leaflet-awesome-markers/leaflet.awesome-markers.min",
        "spin": "vendor/spin/spin.min",
        "mustache": "vendor/mustache/mustache.min",
        "mustacheLoader": "vendor/mustache/jquery.mustache.loader",
        "slimbox": "vendor/slimbox/js/slimbox2",
        "switch": "vendor/switch-button/jquery.switchButton",
        "flickr": "vendor/flickr/flickr",
        "d3": "vendor/dagre-d3/d3.min",
        "dagre-d3": "vendor/dagre-d3/dagre-d3.min",
        "jdataview": "vendor/jdataview/jdataview",
        "jbinary": "vendor/jbinary/jbinary",
		//and now our own stuff
        "sidebar": "js/leaflet-sidebar/js/leaflet-sidebar",
        "sserialize": "js/sserialize/sserialize",
        "oscar": "js/oscar/oscar",
		"storage": "js/storage/storage",
        "tools": "js/tools/tools",
        "conf": "js/config/config",
        "manager": "js/connection/manager.min",
        "tree": "js/tree/tree",
        "map": "js/map/map",
        "state": "js/state/manager",
        "query": "js/query/query",
        "search": "js/search/search",
		"dag": "js/dag/dag",
		"dagexp" : "js/dag/dagexp",
        "kv-clustering" : "js/kv-clustering/kv-clustering"
    },
    shim: {
        'bootstrap': {deps: ['jquery']},
        'leafletCluster': {deps: ['leaflet', 'jquery']},
        'awesomeMarkers': {deps: ['leaflet', 'jquery']},
        'sidebar': {deps: ['leaflet', 'jquery']},
        'mustacheLoader': {deps: ['jquery']},
        'slimbox': {deps: ['jquery']},
    },
    waitSeconds: 20
});

requirejs(["leaflet", "jquery", "mustache", "jqueryui", "sidebar", "mustacheLoader", "conf",  "switch", "state", "map", "tree", "query", "tools", "search"],
    function () {
        var L = require("leaflet");
		var jQuery = require("jquery");
		var mustache = require("mustache");
		var jqueryui = require("jqueryui");
		var sidebar = require("sidebar");
		var mustacheLoader = require("mustacheLoader");
		var config = require("conf");
		var switchButton = require("switch");
		var state = require("state");
		var map = require("map");
		var tree = require("tree");
        var query = require("query");
		var tools = require("tools");
        var search = require("search");
		
		//set the map handler
		state.mapHandler = map;

        // mustache-template-loader needs this
        window.Mustache = mustache;

        // load template files
        $.Mustache.load('template/arrayItemListEntryTemplate.mst');
        $.Mustache.load('template/spatialQueryTableRowTemplate.mst');
        $.Mustache.load('template/treeTemplate.mst');
		$.Mustache.load('template/resultListPaginationTemplate.mst');
        $("#sidebar-pane-help").load('template/help.html', function () {
            $('.example-query-string').on('click', function () {
				state.setQuery(this.firstChild.data.trim());
            });
        });
        $("#sidebar-pane-legal").load('template/legal.html');
        $(document).ready(function () {
            $("#tree").resizable();

            $('[data-toggle="tooltip"]').tooltip();

            $("#search_form").bind('submit', function (e) {
                e.preventDefault();
				$("#search_text").autocomplete("close");
                search.instantCompletion();
            });
			search.bindTagCompletion('#search_text');
			
			$("#searchclear").click(function() {
				var search_text = $("#search_text");
				search_text.val("");
				state.clearViews();
				search_text.focus();
			});
			
            $('#graph').click(function () {
                $("#onePath").button().change(function() {
					//we need to redraw the whole graph if the former state was onePath
					if (state.dag.hasRegion(0xFFFFFFFF) && ! $(this).is(":checked") ) {
						tree.visualizeDAG(state.dag.region(0xFFFFFFFF));
					}
				});
                $("#wholeTree").button().click(function () {
                    map.loadWholeTree();
					if (state.dag.hasRegion(0xFFFFFFFF)) {
						tree.visualizeDAG(state.dag.region(0xFFFFFFFF));
					}
                });
                if (state.dag.hasRegion(0xFFFFFFFF)) {
                    tree.visualizeDAG(state.dag.region(0xFFFFFFFF));
                }
            });

            $('#closeTree a').click(function () {
                state.visualizationActive = false;
                $('#tree').css("display", "none");
            });
			
			$("#download_result").click(function() {
				$('#floatpanel').removeClass("hidden");
			});
			
            $('#floatpanel_close').click(function () {
                $('#floatpanel').addClass("hidden");
            });

            $('#closeFlickr a').click(function () {
                $("#flickr").hide("slide", {direction: "right"}, config.styles.slide.speed);
            });

            $("#searchModi input").switchButton({
                on_label: 'Local',
                off_label: 'Global'
            });

            $('#show_tree').click(function () {
                $('#results_tree_parent').toggle();
            });

            $('#show_flickr').click(function () {
                var flickr = $("#flickr");
                if (!$(this).is(':checked')) {
                    flickr.hide();
                } else {
                    flickr.show();
                }
            });

            $(document).on('click', '#kvclustering-list li.refinement' ,(function () {
               $('#search_text').val(this.id);
               search.instantCompletion();
            }));
			
			$('#display_cluster_shapes_checkbox').click(function() {
				var enabled = $(this).is(':checked');
				map.cfg.clusterShapes.auto = false;
				map.cfg.clusterShapes.preload = enabled;
				map.cfg.clusterShapes.display = enabled;
				map.reloadShapeConfig();
			});
			
			$('#display_item_shapes_checkbox').click(function() {
				var enabled = $(this).is(':checked');
				map.cfg.resultList.showItemShapes = enabled;
				map.mapViewChanged();
			});
			
			$('#display_item_markers_checkbox').click(function() {
				var enabled = $(this).is(':checked');
				map.cfg.resultList.showItemMarkers = enabled;
				map.mapViewChanged();
			});
			
// 			state.sidebar.on('tab-closed', function(e) {});
// 			state.sidebar.on('tab-opened', function(e) {});

            $('#spatialquery_selectbutton').click(function() {
                if (state.spatialquery.selectButtonState === 'select') {
                    query.startSpatialQuery();
                }
                else if (state.spatialquery.selectButtonState === 'finish') {
                    query.endSpatialQuery();
                }
                else if (state.spatialquery.selectButtonState === 'clear') {
                    query.clearSpatialQuery();
                }
            });
			
            $('#spatialquery_acceptbutton').click(function() {
                if (state.spatialquery.type === undefined) {
                    return;
                }
                query.endSpatialQuery();
                var qStr = ""
                if (state.spatialquery.type === "rect") {
                    var minLat = Math.min(state.spatialquery.coords[0].lat, state.spatialquery.coords[1].lat);
                    var maxLat = Math.max(state.spatialquery.coords[0].lat, state.spatialquery.coords[1].lat);
                    var minLng = Math.min(state.spatialquery.coords[0].lng, state.spatialquery.coords[1].lng);
                    var maxLng = Math.max(state.spatialquery.coords[0].lng, state.spatialquery.coords[1].lng);
					//now clip
					var minLat = Math.max(minLat, -90);
					var maxLat = Math.min(maxLat, 90);
					var minLng = Math.max(minLng, -180);
					var maxLng = Math.min(maxLng, 180);
                    qStr = "$geo:" + minLng + "," + minLat + "," + maxLng + "," + maxLat;
                }
                else if (state.spatialquery.type === "path") {
                    if (state.spatialquery.coords.length > 0) {
                        qStr = "$path:" + jQuery('#spatialquery_radius').val();
                        for(let coord of state.spatialquery.coords) {
                            qStr += "," + coord.lat + "," + coord.lng;
                        }
                    }
                }
                else if (state.spatialquery.type === "point") {
                    if (state.spatialquery.coords.length > 0) {
                        qStr = "$point:" + jQuery('#spatialquery_radius').val();
						qStr += "," + state.spatialquery.coords[0].lat + "," + state.spatialquery.coords[0].lng;
                    }
                }
                else if (state.spatialquery.type === "cell") {
                    if (state.spatialquery.coords.length > 0) {
                        qStr = "$cell:"+ state.spatialquery.coords[0].lat + "," + state.spatialquery.coords[0].lng;
                    }
                }
                else if (state.spatialquery.type === "poly") {
                    if (state.spatialquery.coords.length >= 3) {
                        qStr = "$poly";
                        var delim = ":"
                        for(let coord of state.spatialquery.coords) {
                            qStr += delim + coord.lat + "," + coord.lng;
                            delim = ",";
                        }
                    }
                }
                if (qStr.length) {
					var id;
					for(i=0; true; ++i) {
						id = i + ""
						if (!state.spatialObjects.store.count(id)) {
							break;
						}
					}
					var data = {
						name : id,
						type : state.spatialquery.type,
						mapshape : state.spatialquery.mapshape,
						query : qStr
					};
					state.spatialObjects.store.insert(id, data);
					state.spatialObjects.names.insert(id, id);
					map.appendSpatialObjectToTable(id);
                }
                query.clearSpatialQuery();
            });
			
            $('#spatialquery_type').change(function(e) {
                if (e.target.value !== state.spatialquery.type) {
                    query.clearSpatialQuery();
                }
                if (e.target.value === 'path' || e.target.value === 'point') {
//                     $('#spatialquery_radius_group').removeClass('hidden');
                }
                else {
//                     $('#spatialquery_radius_group').addClass('hidden');
                }
            });
			

			
			$("#result_download_select").change(search.updateDownloadLink);
			$("#result_download_format").change(search.updateDownloadLink);

            $(window).bind('popstate', function (e) {
// 				console.log(e);
//                 search.queryFromSearchLocation();
            });

            //check if there's a query in our location string
            search.queryFromSearchLocation();
			
			//store some config options
			
			state.sidebar.open("search");
			$("#search_text").focus();
        });
    });
