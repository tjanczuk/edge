/*global module:false*/
module.exports = function(grunt) {

  // Project configuration.
  grunt.initConfig({
    simplemocha: {
      options: {
        globals: ['should'],
        timeout: 3000,
        ignoreLeaks: false,
        grep: '*-test',
        ui: 'bdd',
        reporter: 'spec'
      },

      all: { src: 'test/**/*.js' }
    },

    jshint: {
      options: {
        curly: true,
        eqeqeq: true,
        immed: false,
        latedef: true,
        newcap: true,
        noarg: true,
        sub: true,
        undef: true,
        boss: true,
        eqnull: true,
        browser: true,
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
  grunt.loadNpmTasks('grunt-rigger');
  grunt.loadNpmTasks('grunt-mocha');
  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-jshint');
  grunt.loadNpmTasks('grunt-contrib-uglify');
  grunt.loadNpmTasks('grunt-simple-watch');
  grunt.loadNpmTasks('grunt-simple-mocha');

  // Default task.
  grunt.registerTask('default', ['jshint', 'simplemocha']);
  grunt.registerTask('test', ['simplemocha']);
  grunt.registerTask('watch-test', ['watch']);
};
