/*global module:false*/
module.exports = function(grunt) {

  // Project configuration.
  grunt.initConfig({
    simplemocha: {
      options: {
        globals: ['should'],
        timeout: 3000,
        ignoreLeaks: false,
        ui: 'bdd',
        reporter: 'spec'
      },

      all: { src: 'test/**/*.js' }
    },

    jshint: {
        options: {
        asi : false,
        bitwise : true,
        boss : false,
        browser : true,
        curly : true,
        debug: false,
        devel: false,
        eqeqeq: true,
        evil: false,
        expr: true,
        forin: false,
        immed: true,
        jquery : true,
        latedef : false,
        laxbreak: false,
        multistr: true,
        newcap: true,
        noarg: true,
        node : true,
        noempty: false,
        nonew: true,
        onevar: false,
        plusplus: false,
        regexp: false,
        strict: false,
        sub: false,
        trailing : true,
        undef: true,
        globals: {
          jQuery: true,
          Backbone: true,
          _: true,
          Marionette: true,
          $: true,
          slice: true
        }
      },
      js: ['./lib/*.js']
    },

    watch: {
      scripts: {
        files: ['**/*.js'],
        tasks: ['test']
      }
    }
  });

  // Laoded tasks
  grunt.loadNpmTasks('grunt-contrib-jshint');
  grunt.loadNpmTasks('grunt-simple-mocha');

  // Default task.
  grunt.registerTask('default', ['jshint', 'simplemocha']);
  grunt.registerTask('test', ['simplemocha']);
  grunt.registerTask('watch-test', ['watch']);
};
