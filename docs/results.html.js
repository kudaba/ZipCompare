Chart.defaults.global.title.display = true;
Chart.defaults.global.legend.position = 'bottom';

/*
var chartConfig = {
  test: "Test Name",
  id: "canvasID",
  parameters: ["0", "1", "2"],
  passes: [
    {
      name: "First Pass"
      results: [100, 200, 800]
      color: "rgba(75,192,192,0.4)",
    }
    {
      name: "Second Pass"
      results: [500, 400, 300]
      color: "rgba(75,192,192,0.4)",
    }
  ]
};
*/
function CreateChart(chartConfig) {
    var data = {
        labels: chartConfig.parameters,
        datasets: []
    };

    chartConfig.passes.forEach(function (e) {
        data.datasets.push({
            label: e.name,
            fill: false,
            lineTension: 0.1,
            backgroundColor: e.color,
            borderColor: e.color,
            borderCapStyle: 'butt',
            borderDash: [],
            borderDashOffset: 0.0,
            borderJoinStyle: 'miter',
            pointBorderColor: e.color,
            pointBackgroundColor: "#fff",
            pointBorderWidth: 1,
            pointHoverRadius: 5,
            pointHoverBackgroundColor: e.color,
            pointHoverBorderColor: "rgba(220,220,220,1)",
            pointHoverBorderWidth: 2,
            pointRadius: 1,
            pointHitRadius: 10,
            data: e.results,
            spanGaps: false
        });
    });

    var options = {
        title: {
            text: chartConfig.test
        },
		scales: {
			yAxes: [{
				type: typeof(chartConfig.type) !== 'undefined' ? chartConfig.type : 'linear',
				max: chartConfig.max
			}]
		}
    };

    var ctx = document.getElementById(chartConfig.id);

    var myLineChart = Chart.Line(ctx, {
        data: data,
        options: options
    });
}


