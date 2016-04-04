module.exports = [
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Account preferences"
      },
      {
        "type": "input",
        "appKey": "username",
        "label": "username"
      },
      {
        "type": "slider",
        "appKey": "icon",
        "defaultValue": 0,
        "label": "Slider",
        "description": "icon ID",
        "min": 0,
        "max": 7,
        "step": 1
      },
      {
        "type": "input",
        "appKey": "message",
        "label": "timeline message"
      },
      {
        "type": "submit",
        "defaultValue": "Save"
      }
    ]
  }
];