module.exports = [
    {
      "type": "heading",
      "defaultValue": "Watchface config"
    },
    {
      "type": "section",
      "capabilities": ["COLOR"],
      "items": [
        {
          "type": "heading",
          "defaultValue": "Colour scheme"
        },
        {
          "type": "radiogroup",
          "messageKey": "colourScheme",
          "label": "Colour scheme // can make watchface hang - if so, choose another face, then reselect this one",
          "defaultValue": "c",
          "options": [
            { 
              "label": "Cyan", 
              "value": "c"
            },
            { 
              "label": "Magenta", 
              "value": "m"
            },
            { 
              "label": "Mixed", 
              "value": "b"
            },
          ]
        }
      ]
    },
    {
      "type": "section",
      "capabilities": ["BW"],
      "items": [
        {
          "type": "heading",
          "defaultValue": "Pebble 1/2 settings"
        },
        {
          "type": "radiogroup",
          "messageKey": "bwBgColor",
          "label": "Background color",
          "defaultValue": "b",
          "options": [
            { 
              "label": "Black", 
              "value": "b"
            },
            { 
              "label": "White", 
              "value": "w"
            }
          ]
        }
      ]
    },
    {
      "type": "section",
      "items": [
        {
          "type": "heading",
          "defaultValue": "Settings"
        },
        {
          "type": "toggle",
          "messageKey": "fahrenheit",
          "label": "Use Fahrenheit",
          "defaultValue": false
        }
      ]
    },
    {
      "type": "section",
      "items": [
        {
          "type": "input",
          "messageKey": "customKey",
          "defaultValue": "",
          "label": "Custom OpenWeatherMap API key",
          "attributes": {
            "placeholder": "Leave blank to use built-in key",
            "limit": 32,
          }
        },
        {
          "type": "input",
          "messageKey": "customLoc",
          "defaultValue": "",
          "label": "Override location",
          "attributes": {
            "placeholder": "Leave blank to use current GPS location",
            "limit": 32,
          }
        },
        {
          "type": "input",
          "messageKey": "secret",
          "defaultValue": "",
          "label": "test",
          "attributes": {
            "placeholder": "",
            "limit": 32,
          }
        }
      ]
    },
    {
      "type": "submit",
      "defaultValue": "Save"
    }
  ];